#include "./StrategyFactory.hpp"
#include "./TradeBots/Crypto/MATradeBot.hpp"          
#include "./TradeBots/Crypto/RSITradeBot.hpp"   
#include "./TradeBots/Crypto/MARSITradeBot.hpp"   
#include "./TradeBots/Crypto/NewsStrategy.hpp"   

// ����������� ���������
inline void registerStrategies() {
    // ����������� ��������� MA
    StrategyFactory::getInstance().registerStrategy(1, []( int user_id, int bot_id, int strategy_id, int broker_id, const std::map<std::string, std::string>& params) {
        return std::make_unique<MA_strategy>(user_id, bot_id, strategy_id, broker_id, params);
        });

    // ����������� ��������� ConcretteTradeBot
    StrategyFactory::getInstance().registerStrategy(2, []( int user_id, int bot_id, int strategy_id, int broker_id, const std::map<std::string, std::string>& params) {
        return std::make_unique<RSITradeBot>( user_id, bot_id, strategy_id, broker_id, params);
        });
    // ����������� ��������� MARSI
    StrategyFactory::getInstance().registerStrategy(3, [](int user_id, int bot_id, int strategy_id, int broker_id, const std::map<std::string, std::string>& params) {
        return std::make_unique<MA_RSI_Strategy>(user_id, bot_id, strategy_id, broker_id, params);
        });
    // ����������� ��������� � ���������
    StrategyFactory::getInstance().registerStrategy(4, [](int user_id, int bot_id, int strategy_id, int broker_id, const std::map<std::string, std::string>& params) {
        return std::make_unique<NewsStrategy>(user_id, bot_id, strategy_id, broker_id, params);
        });
}

