#ifndef BOT_HANDLER_HPP
#define BOT_HANDLER_HPP

#include "./const.hpp"
#include "./StrategyFactory.hpp"
#include "./strategy_registrations.hpp"
#include <memory>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <iostream>
#include <mysql/jdbc.h>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/version.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp> // Используем nlohmann-json

// Предварительные объявления классов
class TradeBot;
class Informer;

// Структура для хранения информации о боте
struct BotInfo {
    int user_id;
    int bot_id;
    std::unique_ptr<TradeBot> bot;
};

// Класс для управления ботами
class BotHandler {
public:
    BotHandler() {
        registerStrategies();
    }

    // Инициализация всех ботов, у которых isRunning = true
    inline void initialize_bots() {
        try {
            sql::Driver* driver = get_driver_instance();
            std::unique_ptr<sql::Connection> con(driver->connect(Constants::my_sql_host, Constants::my_sql_login, Constants::my_sql_password));
            con->setSchema("tradesnake");

            std::unique_ptr<sql::Statement> stmt(con->createStatement());
            std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT * FROM bots WHERE isRunning = TRUE"));

            while (res->next()) {
                int bot_id = res->getInt("id");
                int user_id = res->getInt("user_id");
                int strategy_id = res->getInt("strategy_id");
                int broker_id = res->getInt("broker_id");
                double money = res->getDouble("money");
                double symbol_count = res->getDouble("symbol_count");
                std::string symbol = res->getString("symbol");

                // Извлекаем параметры стратегии из базы данных
                std::map<std::string, std::string> strategy_params;

                // Пример: извлекаем параметры из JSON-столбца (если они хранятся в таком формате)
                std::string strategy_params_json = res->getString("strategy_parameters");
                if (!strategy_params_json.empty()) {
                    try {
                        auto json_value = nlohmann::json::parse(strategy_params_json);
                        if (json_value.is_object()) {
                            for (auto it = json_value.begin(); it != json_value.end(); ++it) {
                                const std::string& key = it.key();
                                const auto& value = it.value();

                                if (value.is_string()) {
                                    strategy_params[key] = value.get<std::string>();
                                }
                                else {
                                    strategy_params[key] = value.dump(); // Преобразуем в строку, если это не строка
                                }
                            }
                        }
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Error parsing strategy parameters for bot " << bot_id << ": " << e.what() << std::endl;
                    }
                }

                // Добавляем обязательные параметры
                strategy_params["bot_id"] = std::to_string(bot_id);
                strategy_params["broker_id"] = std::to_string(broker_id);
                strategy_params["money"] = std::to_string(money);
                strategy_params["symbol_count"] = std::to_string(symbol_count);
                strategy_params["symbol"] = symbol;

                // Запускаем бота
                start_bot(user_id, bot_id, strategy_id, broker_id, strategy_params);
            }
        }
        catch (const sql::SQLException& e) {
            std::cerr << "Error initializing bots: " << e.what() << std::endl;
        }
    }

    // Инициализация одного бота по его ID
    inline void initialize_single_bot(int bot_id) {
        try {
            sql::Driver* driver = get_driver_instance();
            std::unique_ptr<sql::Connection> con(driver->connect(Constants::my_sql_host, Constants::my_sql_login, Constants::my_sql_password));
            con->setSchema("tradesnake");

            std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT * FROM bots WHERE id = ? AND isRunning = TRUE"));
            pstmt->setInt(1, bot_id);
            std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

            if (res->next()) {
                int user_id = res->getInt("user_id");
                int strategy_id = res->getInt("strategy_id");
                int broker_id = res->getInt("broker_id");
                double money = res->getDouble("money");
                double symbol_count = res->getDouble("symbol_count");
                std::string symbol = res->getString("symbol");

                // Извлекаем параметры стратегии из базы данных
                std::map<std::string, std::string> strategy_params;

                // Пример: извлекаем параметры из JSON-столбца (если они хранятся в таком формате)
                std::string strategy_params_json = res->getString("strategy_parameters");
                if (!strategy_params_json.empty()) {
                    try {
                        auto json_value = nlohmann::json::parse(strategy_params_json);
                        if (json_value.is_object()) {
                            for (auto it = json_value.begin(); it != json_value.end(); ++it) {
                                const std::string& key = it.key();
                                const auto& value = it.value();

                                if (value.is_string()) {
                                    strategy_params[key] = value.get<std::string>();
                                }
                                else {
                                    strategy_params[key] = value.dump(); // Преобразуем в строку, если это не строка
                                }
                            }
                        }
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Error parsing strategy parameters for bot " << bot_id << ": " << e.what() << std::endl;
                    }
                }

                // Добавляем обязательные параметры
                strategy_params["bot_id"] = std::to_string(bot_id);
                strategy_params["broker_id"] = std::to_string(broker_id);
                strategy_params["money"] = std::to_string(money);
                strategy_params["symbol_count"] = std::to_string(symbol_count);
                strategy_params["symbol"] = symbol;

                // Запускаем бота
                start_bot(user_id, bot_id, strategy_id, broker_id, strategy_params);
            }
            else {
                std::cerr << "Bot " << bot_id << " not found or is not running." << std::endl;
            }
        }
        catch (const sql::SQLException& e) {
            std::cerr << "Error initializing bot " << bot_id << ": " << e.what() << std::endl;
        }
    }


    // Запуск бота
    inline void start_bot(int user_id, int bot_id, int strategy_id, int broker_id, const std::map<std::string, std::string>& strategy_params) {
        std::unique_ptr<TradeBot> bot;

        try {
            // Создаём стратегию с использованием параметров
            bot = StrategyFactory::getInstance().createStrategy(strategy_id, user_id, bot_id, broker_id, strategy_params);
        }
        catch (const std::exception& e) {
            std::cerr << "Error creating bot: " << e.what() << std::endl;
            return;
        }

        BotInfo bot_info{ user_id, bot_id, std::move(bot) };

        {
            std::lock_guard<std::mutex> lock(bots_mutex_);
            active_bots_[bot_id] = std::move(bot_info);
        }

        std::thread([bot_id, this]() {
            try {
                if (active_bots_.find(bot_id) != active_bots_.end()) {
                    active_bots_[bot_id].bot->start();
                    std::cout << "Bot " << bot_id << " started successfully." << std::endl;
                }
                else {
                    std::cerr << "Bot " << bot_id << " not found in active bots." << std::endl;
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Error in bot " << bot_id << " thread: " << e.what() << std::endl;
            }
            }).detach();

        std::cout << "Bot " << bot_id << " for user " << user_id << " started with strategy " << strategy_id << "." << std::endl;
    }

    // Остановка бота
    inline int stop_bot(int bot_id) {
        std::lock_guard<std::mutex> lock(bots_mutex_);
        auto it = active_bots_.find(bot_id);
        if (it != active_bots_.end()) {
            it->second.bot->stop();
            active_bots_.erase(it);
            std::cout << "Bot " << bot_id << " stopped." << std::endl;
            return 1;
        }
        else {
            std::cout << "Bot " << bot_id << " not found." << std::endl;
            return 0;
        }
    }
    std::vector<HistoricalResult> start_execute_historical(
        int user_id, int bot_id, int strategy_id, int broker_id,
        const std::map<std::string, std::string>& strategy_params) {


        std::unique_ptr<TradeBot> bot = StrategyFactory::getInstance().createStrategy(
            strategy_id, user_id, bot_id, broker_id, strategy_params);

        std::vector<HistoricalResult> results = bot->execute_historical();
        return results;
    }
private:
    std::map<int, BotInfo> active_bots_;  // Ключ - bot_id
    std::mutex bots_mutex_;
    std::shared_ptr<Informer> informer_;
};

#endif // BOT_HANDLER_HPP