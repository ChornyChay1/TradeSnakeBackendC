#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <sstream>
#include <map>
#include <memory>
#include <mutex>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/version.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp> // Используем nlohmann-json
#include "./Informers/Crypto/ByBitInformer.hpp"
#include "./TradeBots/Crypto/RSITradeBot.hpp"
#include "./TradeBots/Crypto/MATradeBot.hpp"
#include <mysql/jdbc.h>
#include "./BotHandler.hpp"
#include <boost/asio/ip/address.hpp>
#include <set>
#include "./const.hpp"
#include "./struct.hpp"
#include "./Analyzers/MarketAnalyzer.hpp"


using json = nlohmann::json; // Используем nlohmann::json
namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
using json = nlohmann::json; // Используем nlohmann::json

// Функция для парсинга JSON из тела запроса
std::map<std::string, std::string> parse_json_body(const std::string& body) {
    std::map<std::string, std::string> result;
    try {
        auto json_value = json::parse(body); // Парсим JSON
        if (json_value.is_object()) {
            for (auto it = json_value.begin(); it != json_value.end(); ++it) {
                const std::string& key = it.key();
                const auto& value = it.value();

                if (value.is_string()) {
                    result[key] = value.get<std::string>();
                }
                else {
                    result[key] = value.dump(); // Преобразуем в строку, если это не строква
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
    }
    return result;
}

bool is_allowed_ip(const boost::asio::ip::tcp::endpoint& endpoint) {
    std::string client_ip = endpoint.address().to_string();
    return Constants::allowed_ips.find(client_ip) != Constants::allowed_ips.end();
}
void handle_execute_historical(http::request<http::string_body>& req, http::response<http::string_body>& res, BotHandler& bot_handler) {
    auto params = parse_json_body(req.body());

    if (params.find("user_id") == params.end() ||
        params.find("strategy_id") == params.end() || params.find("broker_id") == params.end() ||
        params.find("strategy_parameters") == params.end()) {

        res.result(http::status::bad_request);
        res.set(http::field::content_type, "text/plain");
        res.body() = "Missing required parameters: user_id, strategy_id, broker_id, start_date, end_date, or strategy_parameters.";
        return;
    }

    // Извлекаем параметры из JSON
    int user_id = std::stoi(params["user_id"]);
    int bot_id = -1;   
    int strategy_id = std::stoi(params["strategy_id"]);
    int broker_id = std::stoi(params["broker_id"]);

    // Параметры стратегии
    std::map<std::string, std::string> strategy_params;
    if (params.find("strategy_parameters") != params.end()) {
        auto strategy_params_json = json::parse(params["strategy_parameters"]);
        for (auto it = strategy_params_json.begin(); it != strategy_params_json.end(); ++it) {
            const std::string& key = it.key();
            const auto& value = it.value();
            strategy_params[key] = value.is_string() ? value.get<std::string>() : value.dump();
        }
    }

    // Запускаем бэктестинг и получаем результаты
    std::vector<HistoricalResult> results = bot_handler.start_execute_historical(
        user_id, bot_id, strategy_id, broker_id, strategy_params);

    json response_json = json::array();
    for (const auto& result : results) {
        response_json.push_back({
            {"timestamp", result.timestamp},
            {"open", result.open},
            {"close", result.close},
            {"high", result.high},
            {"low", result.low},
            {"volume", result.volume},
            {"turnover",result.turnover},
            {"buy", result.buy},
            {"sell", result.sell}
            });
    }

    res.result(http::status::ok);
    res.set(http::field::content_type, "application/json");
    res.body() = response_json.dump();
    res.prepare_payload();
}

void handle_start(http::request<http::string_body>& req, http::response<http::string_body>& res, BotHandler& bot_handler) {
    // Парсим JSON из тела запроса
    auto params = parse_json_body(req.body());

    // Проверяем обязательные параметры
    if (params.find("user_id") == params.end() || params.find("bot_id") == params.end() ||
        params.find("strategy_id") == params.end() || params.find("broker_id") == params.end()
        || params.find("money") == params.end() || params.find("symbol") == params.end()
        ) {
        res.result(http::status::bad_request);
        res.set(http::field::content_type, "text/plain");
        res.body() = "Missing required parameters: user_id, bot_id, strategy_id, or broker_id.";
        return;
    }

    try {
        int user_id = std::stoi(params["user_id"]);
        std::string symbol = params["symbol"];

        int bot_id = std::stoi(params["bot_id"]);
        int strategy_id = std::stoi(params["strategy_id"]);
        int broker_id = std::stoi(params["broker_id"]);
        std::string money = params["money"];

        // Параметры стратегии (если есть)
        std::map<std::string, std::string> strategy_params;
        if (params.find("strategy_parameters") != params.end()) {
            auto strategy_params_json = json::parse(params["strategy_parameters"]);
            if (strategy_params_json.is_object()) {
                for (auto it = strategy_params_json.begin(); it != strategy_params_json.end(); ++it) {
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
        strategy_params["money"] = money;
        strategy_params["symbol"] = symbol;

        // Запуск бота
        bot_handler.start_bot(user_id, bot_id, strategy_id, broker_id, strategy_params);

        res.result(http::status::ok);
        res.set(http::field::content_type, "text/plain");
        res.body() = "Bot " + std::to_string(bot_id) + " for user " + std::to_string(user_id) + " started.";
    }
    catch (const std::exception& e) {
        res.result(http::status::bad_request);
        res.set(http::field::content_type, "text/plain");
        res.body() = "Invalid parameter format: " + std::string(e.what());
    }
}

void handle_stop(http::request<http::string_body>& req, http::response<http::string_body>& res, BotHandler& bot_handler) {
    // Парсим JSON из тела запроса
    auto params = parse_json_body(req.body());

    // Проверяем обязательные параметры
    if (params.find("bot_id") == params.end()) {
        res.result(http::status::bad_request);
        res.set(http::field::content_type, "text/plain");
        res.body() = "Missing required parameter: bot_id.";
        return;
    }

    try {
        int bot_id = std::stoi(params["bot_id"]);

        bot_handler.stop_bot(bot_id);
            res.result(http::status::ok);
            res.set(http::field::content_type, "text/plain");
            res.body() = "Bot " + std::to_string(bot_id) + " stopped.";

    }
    catch (const std::exception& e) {
        res.result(http::status::bad_request);
        res.set(http::field::content_type, "text/plain");
        res.body() = "Invalid parameter format: " + std::string(e.what());
    }
}
void handle_update(http::request<http::string_body>& req, http::response<http::string_body>& res, BotHandler& bot_handler) {
    // Парсим JSON из тела запроса
    auto params = parse_json_body(req.body());

    // Проверяем обязательные параметры
    if (params.find("user_id") == params.end() || params.find("bot_id") == params.end() ||
        params.find("strategy_id") == params.end() || params.find("broker_id") == params.end()) {
        res.result(http::status::bad_request);
        res.set(http::field::content_type, "text/plain");
        res.body() = "Missing required parameters: user_id, bot_id, strategy_id, or broker_id.";
        return;
    }

    try {
        int user_id = std::stoi(params["user_id"]);
        int bot_id = std::stoi(params["bot_id"]);
        int strategy_id = std::stoi(params["strategy_id"]);
        int broker_id = std::stoi(params["broker_id"]);
        std::string money = params["money"];
        std::string symbol = params["money"];

        // Параметры стратегии (если есть)
        std::map<std::string, std::string> strategy_params;
        if (params.find("strategy_parameters") != params.end()) {
            auto strategy_params_json = json::parse(params["strategy_parameters"]);
            if (strategy_params_json.is_object()) {
                for (auto it = strategy_params_json.begin(); it != strategy_params_json.end(); ++it) {
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
        strategy_params["money"] = money;
        strategy_params["symbol"] = symbol;


        bot_handler.stop_bot(bot_id);

        bot_handler.start_bot(user_id, bot_id, strategy_id, broker_id, strategy_params);

        res.result(http::status::ok);
        res.set(http::field::content_type, "text/plain");
        res.body() = "Bot " + std::to_string(bot_id) + " for user " + std::to_string(user_id) + " updated and restarted.";
    }
    catch (const std::exception& e) {
        res.result(http::status::bad_request);
        res.set(http::field::content_type, "text/plain");
        res.body() = "Invalid parameter format: " + std::string(e.what());
    }
}
void handle_analyze(http::request<http::string_body>& req, http::response<http::string_body>& res, BotHandler& bot_handler) {
    try {
        // Парсим JSON из тела запроса
        auto params = parse_json_body(req.body());

        // Проверяем наличие обязательных параметров
        if (params.find("start_date") == params.end() ||
            params.find("end_date") == params.end() ||
            params.find("market_type_name") == params.end() ||
            params.find("symbol") == params.end()) {

            res.result(http::status::bad_request);
            res.set(http::field::content_type, "application/json");
            json response_json = { {"error", "Missing required parameters: start_date, end_date, market_type_name, or symbol."} };
            res.body() = response_json.dump();
            res.prepare_payload();
            return;
        }

        std::string start_date = params["start_date"];
        std::string end_date = params["end_date"];
        std::string symbol = params["symbol"];
        std::string market_type_name = params["market_type_name"];

        std::shared_ptr<Informer> informer;
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
                    res.result(http::status::bad_request);
                    res.set(http::field::content_type, "application/json");
                    json response_json = { {"error", "Invalid market_type_name. Supported: Crypto, Stocks."} };
                    res.body() = response_json.dump();
                    res.prepare_payload();
                    return;
                }
            }
        }
      

        // Создаём анализатор и проводим анализ
        MarketAnalyzer analyzer(informer);
        MarketState result = analyzer.analyze_market(symbol, start_date, end_date);

        // Формируем JSON-ответ
        json response_json;
        response_json["trend"] = result.trend;
        response_json["is_trend"] = result.is_trend;
        response_json["volatility"] = result.volatility;
        response_json["volatility_persent"] = result.volatility_percent;

        response_json["max_price"] = result.max_price;
        response_json["min_price"] = result.min_price;

        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = response_json.dump();
        res.prepare_payload();
    }
    catch (const std::exception& e) {
        res.result(http::status::internal_server_error);
        res.set(http::field::content_type, "application/json");
        json response_json = { {"error", "Error processing request: " + std::string(e.what())} };
        res.body() = response_json.dump();
        res.prepare_payload();
    }
}
void handle_data_historical(http::request<http::string_body>& req, http::response<http::string_body>& res, BotHandler& bot_handler) {
    try {
        // Парсим JSON из тела запроса
        auto params = parse_json_body(req.body());

        // Проверяем наличие обязательных параметров
        if (params.find("start_date") == params.end() ||
            params.find("end_date") == params.end() ||
            params.find("market_type_name") == params.end() ||
            params.find("symbol") == params.end() ||
            params.find("interval") == params.end()) {  // Исправлено: interval вместо intreval

            res.result(http::status::bad_request);
            res.set(http::field::content_type, "application/json");
            json response_json = { {"error", "Missing required parameters: start_date, end_date, market_type_name, symbol, or interval."} };
            res.body() = response_json.dump();
            res.prepare_payload();
            return;
        }

        std::string start_date = params["start_date"];
        std::string end_date = params["end_date"];
        std::string symbol = params["symbol"];
        std::string market_type_name = params["market_type_name"];
        std::string interval = params["interval"];

        // Проверка на пустые значения
        if (start_date.empty() || end_date.empty() || symbol.empty() || interval.empty()) {
            res.result(http::status::bad_request);
            res.set(http::field::content_type, "application/json");
            json response_json = { {"error", "Parameters cannot be empty."} };
            res.body() = response_json.dump();
            res.prepare_payload();
            return;
        }

        std::shared_ptr<Informer> informer;
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
                    res.result(http::status::bad_request);
                    res.set(http::field::content_type, "application/json");
                    json response_json = { {"error", "Invalid market_type_name. Supported: Crypto."} };
                    res.body() = response_json.dump();
                    res.prepare_payload();
                    return;
                }

            }
        }


        // Получаем исторические данные
        std::vector<CandleData> result = informer->get_symbol_historical(symbol, start_date, end_date, interval);
        std::sort(result.begin(), result.end(), [](const CandleData& a, const CandleData& b) {
            return a.timestamp < b.timestamp;
            });
        // Преобразуем CandleData в JSON
        json candles_json = json::array();
        for (const auto& candle : result) {
            candles_json.push_back({
                {"timestamp", candle.timestamp},
                {"open", candle.open},
                {"close", candle.close},
                {"high", candle.high},
                {"low", candle.low},
                {"volume", candle.volume}
                });
        }

        // Формируем JSON-ответ
        json response_json;
        response_json["result"] = candles_json;

        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = response_json.dump();
        res.prepare_payload();
    }
    catch (const std::exception& e) {
        // Логируем ошибку
        std::cerr << "Error in handle_data_historical: " << e.what() << std::endl;

        res.result(http::status::internal_server_error);
        res.set(http::field::content_type, "application/json");
        json response_json = { {"error", "Error processing request: " + std::string(e.what())} };
        res.body() = response_json.dump();
        res.prepare_payload();
    }
}
void handle_continue(http::request<http::string_body>& req, http::response<http::string_body>& res, BotHandler& bot_handler) {
    // Парсим JSON из тела запроса
    auto params = parse_json_body(req.body());

    // Проверяем обязательные параметры
    if (params.find("bot_id") == params.end()) {
        res.result(http::status::bad_request);
        res.set(http::field::content_type, "text/plain");
        res.body() = "Missing required parameter: bot_id.";
        return;
    }

    try {
        int bot_id = std::stoi(params["bot_id"]);

        // Продолжаем работу бота
        bot_handler.initialize_single_bot(bot_id);

        res.result(http::status::ok);
        res.set(http::field::content_type, "text/plain");
        res.body() = "Bot " + std::to_string(bot_id) + " continued.";
    }
    catch (const std::exception& e) {
        res.result(http::status::bad_request);
        res.set(http::field::content_type, "text/plain");
        res.body() = "Invalid parameter format: " + std::string(e.what());
    }
}
 
void handle_request(http::request<http::string_body>& req, http::response<http::string_body>& res, BotHandler& bot_handler, const boost::asio::ip::tcp::endpoint& client_endpoint) {
    if (!is_allowed_ip(client_endpoint)) {
        res.result(http::status::forbidden);
        res.set(http::field::content_type, "text/plain");
        res.body() = "Access denied. Your IP is not allowed.";
        return;
    }

    try {
        if (req.target() == "/execute_historical" && req.method() == http::verb::post) {
            handle_execute_historical(req, res, bot_handler);
        }
        else if (req.target() == "/start" && req.method() == http::verb::post) {
            handle_start(req, res, bot_handler);
        }
        else if (req.target() == "/historical_data" && req.method() == http::verb::post) {
            handle_data_historical(req, res, bot_handler);
        }
        else if (req.target() == "/continue" && req.method() == http::verb::post) {
            handle_continue(req, res, bot_handler);
        }
        else if (req.target() == "/stop" && req.method() == http::verb::post) {
            handle_stop(req, res, bot_handler);
        }
        else if (req.target() == "/analyze" && req.method() == http::verb::post) {
            handle_analyze(req, res, bot_handler);
        }
        else if (req.target() == "/update" && req.method() == http::verb::post) {
            handle_update(req, res, bot_handler);
        }
 
        else {
            res.result(http::status::not_found);
            res.set(http::field::content_type, "text/plain");
            res.body() = "Not Found";
        }
    }
    catch (const std::exception& e) {
        res.result(http::status::internal_server_error);
        res.set(http::field::content_type, "text/plain");
        res.body() = "Error: " + std::string(e.what());
    }
}

// Запуск сервера
void run_server() {
    try {
        net::io_context ioc;
        tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), 9090));

        BotHandler bot_handler;
        bot_handler.initialize_bots();

        std::cout << "Server is running on port 9090..." << std::endl;

        while (true) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);

            // Получаем IP-адрес клиента
            boost::asio::ip::tcp::endpoint client_endpoint = socket.remote_endpoint();

            beast::flat_buffer buffer;
            http::request<http::string_body> req;
            http::read(socket, buffer, req);

            http::response<http::string_body> res;

            // Обработка запроса
            handle_request(req, res, bot_handler, client_endpoint);

            http::write(socket, res);
            socket.shutdown(tcp::socket::shutdown_both);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error in server: " << e.what() << std::endl;
    }
}

// Основная функция
int main() {
    setlocale(LC_ALL, "Rus");
    run_server();
    return 0;
}