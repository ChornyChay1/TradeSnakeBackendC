#include <cmath>
#include <vector>
#include <numeric>
#include <algorithm> // для std::max и std::min

struct MarketState {
    std::string trend;  // "Рост", "Падение", "Флэт"
    bool is_trend;
    double volatility;  // Волатильность (стандартное отклонение)
    double normalized_volatility;  // Нормализованная волатильность [0, 1]
    double volatility_percent;  // Волатильность в процентах
    double max_price;
    double min_price;
};

class MarketAnalyzer {
public:
    MarketAnalyzer(std::shared_ptr<Informer> informer) : informer(informer) {}

    MarketState analyze_market(const std::string& symbol, const std::string& start_date, const std::string& end_date) {
        std::vector<CandleData> candles = informer->get_symbol_historical(symbol, start_date, end_date, "60");
        MarketState state;

        if (candles.size() < 5) {
            return state;  // Недостаточно данных для анализа
        }

        std::vector<double> close_prices;
        double max_price = candles[0].close, min_price = candles[0].close;
        double price_change_sum = 0;

        // Определяем тренд через изменение цен и макс/мин значения
        for (size_t i = 1; i < candles.size(); ++i) {
            double close = candles[i].close;
            close_prices.push_back(close);
            max_price = std::max(max_price, close);
            min_price = std::min(min_price, close);

            double change = close - candles[i - 1].close;
            price_change_sum += change;
        }

        // Вычисляем волатильность (стандартное отклонение)
        double mean = std::accumulate(close_prices.begin(), close_prices.end(), 0.0) / close_prices.size();
        double variance = 0;
        for (double price : close_prices) variance += (price - mean) * (price - mean);
        double volatility = std::sqrt(variance / close_prices.size());

        // Нормализация волатильности в диапазоне [0, 1]
        double price_range = max_price - min_price;
        double normalized_volatility = (price_range == 0) ? 0 : (volatility / price_range);

        // Перевод в проценты
        double volatility_percent = normalized_volatility * 100;

        // Анализ скользящей средней (SMA) для тренда
        double sma_short = std::accumulate(close_prices.end() - 3, close_prices.end(), 0.0) / 3;
        double sma_long = std::accumulate(close_prices.begin(), close_prices.end(), 0.0) / close_prices.size();

        if (price_change_sum > 0 && sma_short > sma_long) {
            state.trend = "Up";
        }
        else if (price_change_sum < 0 && sma_short < sma_long) {
            state.trend = "Down";
        }
        else {
            state.trend = "Flat";
        }

        state.is_trend = (volatility > 1.5) && (std::abs(price_change_sum) > (max_price - min_price) * 0.3);
        state.volatility = volatility;
        state.normalized_volatility = normalized_volatility;
        state.volatility_percent = volatility_percent;
        state.max_price = max_price;
        state.min_price = min_price;

        return state;
    }

private:
    std::shared_ptr<Informer> informer;
};