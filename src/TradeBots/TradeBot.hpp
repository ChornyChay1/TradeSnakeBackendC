#ifndef TRADEBOT_HPP
#define TRADEBOT_HPP

#include "../Informers/Informer.hpp"
#include "../Informers/Stocks//TinkoffInformer.hpp"
#include "../Informers/Forex/YahooInformerForex.hpp"
#include <chrono>
#include <memory>
#include <vector>
#include <atomic>
#include <string>
#include <chrono>
#include <thread>
#include <iostream>
#include "../Indicators/Indicator.hpp"
#include "../const.hpp"
#include <mysql/jdbc.h>
#include "../Brokers/Broker.hpp"
#include "../struct.hpp"
#include <algorithm>

// Абстрактный класс TradeBot
class TradeBot {
protected:
    std::atomic<bool> is_running;
    std::shared_ptr<Informer> informer;
    std::shared_ptr<sql::Connection> con;
    std::string symbol;
    int user_id;
    int money;
    int bot_id;
    int strategy_id;
    int broker_id;
    double count_of_symbol;
    std::string interval;
    std::string last_operation = "SELL";
    std::shared_ptr<IndicatorsCalc> indicator;
    std::shared_ptr<Broker> broker;
    std::map<std::string, std::string> params; // Храним параметры как копию
    std::string position = "sell";
    std::string start_date = "";
    std::string end_date = get_current_timestamp();
    std::condition_variable cv_;
    std::mutex cv_mutex_;

    // Метод для установления соединения с БД
    std::chrono::seconds get_sleep_duration(const std::string& interval) {
        if (interval == "d") return std::chrono::hours(24);
        if (interval == "15") return std::chrono::minutes(15);
        if (interval == "60") return std::chrono::hours(1);
        if (interval == "30") return std::chrono::minutes(30);
        if (interval == "5") return std::chrono::minutes(5);
        if (interval == "1") return std::chrono::minutes(1);
        return std::chrono::seconds(1);
    }

    std::string get_current_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto now_s = std::chrono::time_point_cast<std::chrono::seconds>(now);
        auto epoch = now_s.time_since_epoch();
        auto value = std::chrono::duration_cast<std::chrono::seconds>(epoch);
        return std::to_string(value.count());
    }
 


    void initialize_informer_from_db() {
        if (!con) {
            std::cerr << "Ошибка: Нет соединения с БД\n";
            return;
        }

        try {
            std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
                "SELECT markettypes.market_type_name "
                "FROM brokers "
                "INNER JOIN markets ON markets.id = brokers.market_id "
                "INNER JOIN markettypes ON markets.market_type_id = markettypes.id "
                "WHERE brokers.id = ?"
            ));
            pstmt->setInt(1, broker_id);
            std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

            if (res->next()) {
                std::string market_type_name = res->getString("market_type_name");
                if (market_type_name == "Crypto") {
                    informer = std::make_shared<ByBitInformer>();
                }
                else {

                    if (market_type_name == "Stocks") {
                        informer = std::make_shared<TinkoffInformer>(Constants::tinkoff_token);
                    }
                    else {

                        if (market_type_name == "Forex") {
                            informer = std::make_shared<YahooForexInformer>();
                        }
                        else {
                            informer = std::make_shared<ByBitInformer>();

                        }
                    }
                } 
            }
        }
        catch (sql::SQLException& e) {
            std::cerr << "Ошибка SQL: " << e.what() << "\n";
        }
    }
public:
    TradeBot(
        int user_id,
        int bot_id,
        int strategy_id,
        int broker_id,
        const std::map<std::string, std::string>& params
    ) :
        user_id(user_id),
        bot_id(bot_id),
        strategy_id(strategy_id),
        broker_id(broker_id),
        symbol(params.at("symbol")),
        params(params),
        is_running(false) {
        con = Constants::createConnection();
        initialize_informer_from_db();
        indicator = std::make_shared<IndicatorsCalc>(this->informer);
        broker = std::make_shared<Broker>(broker_id);
        // Инициализируем ma_length и interval, если они есть в параметрах
        money = (params.find("money") != params.end()) ? std::stoi(params.at("money")) :  0;
        interval = (params.find("interval") != params.end()) ? params.at("interval") : "d";

        count_of_symbol = (params.find("symbol_count") != params.end()) ? std::stoi(params.at("symbol_count")) : 0;

    }
    void start() {
        if (!con) return;

        is_running.store(true);
        double quantity = 0;
        double real_price;
        while (is_running.load()) {
            double current_price = informer->get_symbol_now(symbol);
            int res = strategy(current_price);
            if (res == 1) {
                quantity = money / current_price;
                if (quantity != 0) {
                real_price = broker->calculateRealPriceBuy(current_price, quantity);
                quantity = money / real_price;
                    broker->buy(bot_id, current_price,real_price, quantity);
                    count_of_symbol += quantity;
                    money = 0;
                }
            }
            else {
                if (res == -1) {
                    quantity = count_of_symbol;
                    if (quantity != 0) {
                    real_price = broker->calculateRealPriceSell(current_price, quantity);
                        broker->sell(bot_id, current_price, real_price, quantity);
                        count_of_symbol = 0;
                        money = quantity * current_price;
                    }
                }
                else {
                    quantity = 1;
                    real_price = broker->calculateRealPriceSell(current_price,quantity);
                    broker->hold(bot_id, real_price);
                }
            }



            std::unique_lock<std::mutex> lock(cv_mutex_);
            cv_.wait_for(lock, get_sleep_duration(interval), [this]() {
                return !is_running.load();
                });

            if (!is_running.load()) {
                break;
            }
        }
    }
    // Метод для выполнения стратегии на исторических данных
    std::vector<HistoricalResult> execute_historical() {
        std::vector<HistoricalResult> results;
        auto start_time = std::chrono::high_resolution_clock::now();

        // Получаем начальную и конечную даты из параметров
         start_date = (params.find("start_date") != params.end()) ? params.at("start_date") : "0";
         end_date = (params.find("end_date") != params.end()) ? params.at("end_date") : "0";

         double quantity = 0;
         double real_price;
        // Получаем исторические данные
        std::vector<CandleData> candles = informer->get_symbol_historical(symbol, start_date, end_date, interval);
        std::sort(candles.begin(), candles.end(), [](const CandleData& a, const CandleData& b) {
            return a.timestamp < b.timestamp;
            });

        int broker_price;
        // Обрабатываем каждую свечу
        for (size_t i = 0; i < candles.size(); ++i) {

            const auto& candle = candles[i];
            double price = candle.close; // Используем цену закрытия для стратегии

            // Создаем результат для текущей свечи
            HistoricalResult result = {
                candle.timestamp,
                candle.open,
                candle.close,
                candle.high,
                candle.low,
                candle.volume,
                candle.turnover,
                {},  
                {}
            };
            end_date = candle.timestamp;
            if (end_date.size() >= 3) {
                end_date.erase(end_date.size() - 3);
            }
            // Применяем стратегию
            int res = strategy(price);
            // Логика для покупки/продажи
            if (res == 1) {
                quantity = money / price;
                if (!quantity == 0) {
                    real_price = broker->calculateRealPriceBuy(price, quantity);
                    quantity = money / real_price;
                    result.buy = { {"price", price * quantity},{"broker_price", real_price * quantity}, {"quantity", quantity} }; // Пример количества
                    position = "buy";
                    count_of_symbol += quantity;
                    money = 0;
                }

            }
            if (res == -1) {
                quantity = count_of_symbol;
                if (!quantity == 0) {
                real_price = broker->calculateRealPriceSell(price, quantity);
                    result.sell = { {"price", price * quantity},
                        {"broker_price", real_price * quantity},
                        { "quantity",quantity } }; // Пример количества
                    position = "sell";                    count_of_symbol = 0;
                    money = quantity * price;
                }
            }
 

            results.push_back(result);

        }
        // Запоминаем время окончания выполнения метода
        auto end_time = std::chrono::high_resolution_clock::now();

        // Вычисляем продолжительность выполнения
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        // Выводим время выполнения в консоль
        std::cout << "Execution time of execute_historical: " << duration << " milliseconds\n";

        return results;
    }
    

    void stop() {
        is_running.store(false);
        cv_.notify_all();
    } 
    //1:купить -1продать 0 ничего не делать
    virtual int strategy(double price) = 0;

    
    virtual ~TradeBot() = default;
};

#endif // TRADEBOT_HPP