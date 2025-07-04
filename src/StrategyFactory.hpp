#ifndef STRATEGY_FACTORY_HPP
#define STRATEGY_FACTORY_HPP

#include <memory>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <string>
#include <map>

// Предварительные объявления классов
class TradeBot;
class Informer;

// Псевдоним типа для фабричной функции
using BotFactory = std::function<std::unique_ptr<TradeBot>( int user_id, int bot_id, int strategy_id, int broker_id, const std::map<std::string, std::string>&)>;

// Фабрика стратегий
class StrategyFactory {
public:
    // Получение экземпляра синглтона
    static StrategyFactory& getInstance() {
        static StrategyFactory instance;
        return instance;
    }

    // Регистрация стратегии
    void registerStrategy(int strategy_id, BotFactory factory) {
        strategies_[strategy_id] = std::move(factory);
    }

    // Создание стратегии
    std::unique_ptr<TradeBot> createStrategy(
        int strategy_id,
        int user_id,
        int bot_id,
        int broker_id,
        const std::map<std::string, std::string>& params
    ) const {
        auto it = strategies_.find(strategy_id);
        if (it != strategies_.end()) {
            return it->second(user_id, bot_id, strategy_id, broker_id, params);
        }
        throw std::runtime_error("Unknown strategy ID");
    }

private:
    // Приватный конструктор для синглтона
    StrategyFactory() = default;

    // Удаление конструктора копирования и оператора присваивания
    StrategyFactory(const StrategyFactory&) = delete;
    StrategyFactory& operator=(const StrategyFactory&) = delete;

    // Хранилище зарегистрированных стратегий
    std::unordered_map<int, BotFactory> strategies_;
};

#endif // STRATEGY_FACTORY_HPP