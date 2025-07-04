#ifndef INDICATORSCALC_HPP
#define INDICATORSCALC_HPP

#include "../Informers/Informer.hpp"
#include <memory>
#include <vector>
#include <numeric>
#include <cmath>
#include <chrono>
#include <iostream>
#include <algorithm>
#include "../struct.hpp"

class IndicatorsCalc {
private:
    std::shared_ptr<Informer> informer;

    std::string get_past_timestamp(const std::string& interval, int periods, std::string end_date) {
        long long end_time = std::stoll(end_date);
        int interval_seconds = 0;

        if (interval == "1") interval_seconds = 60;
        else if (interval == "5") interval_seconds = 5 * 60;
        else if (interval == "15") interval_seconds = 15 * 60;
        else if (interval == "60") interval_seconds = 60 * 60;
        else if (interval == "d") interval_seconds = 24 * 60 * 60;
        else {
            std::cerr << "Unsupported interval: " << interval << std::endl;
            return "";
        }

        long long start_time = end_time - periods * interval_seconds;
        return std::to_string(start_time);
    }

    std::vector<double> extract_close_prices(const std::vector<CandleData>& candles) {
        std::vector<double> close_prices;
        close_prices.reserve(candles.size());
        for (const auto& candle : candles) {
            close_prices.push_back(candle.close);
        }
        return close_prices;
    }

    std::vector<double> calculate_price_changes(const std::vector<double>& prices) {
        std::vector<double> changes;
        for (size_t i = 1; i < prices.size(); ++i) {
            changes.push_back(prices[i] - prices[i - 1]);
        }
        return changes;
    }

public:
    explicit IndicatorsCalc(std::shared_ptr<Informer> informer) : informer(informer) {}

    double calculate_ma(const std::string& symbol, int ma_length, const std::string& interval, std::string end_date) {
        std::string start_date = get_past_timestamp(interval, ma_length+500, end_date);
        std::vector<CandleData> candles = informer->get_symbol_historical(symbol, start_date, end_date, interval);
        std::vector<double> close_prices = extract_close_prices(candles);

        if (close_prices.size() < ma_length) {
            std::cerr << "Недостаточно данных для расчета MA" << std::endl;
            return 0.0;
        }
        if (close_prices.size() > static_cast<size_t>(ma_length)) {
            close_prices.erase(close_prices.begin(), close_prices.end() - (ma_length));
        }

        return std::accumulate(close_prices.end() - ma_length, close_prices.end(), 0.0) / ma_length;
    }

    double calculate_rsi(const std::string& symbol, int rsi_period, const std::string& interval, std::string end_date) {
        std::string start_date = get_past_timestamp(interval, rsi_period + 500, end_date);
        std::vector<CandleData> candles = informer->get_symbol_historical(symbol, start_date, end_date, interval);

        if (candles.size() <= static_cast<size_t>(rsi_period)) {
            std::cerr << "Недостаточно данных для расчета RSI" << std::endl;
            return 50.0;
        }

        std::vector<double> close_prices = extract_close_prices(candles);
        std::vector<double> changes = calculate_price_changes(close_prices);

        // Оставляем только нужное количество данных
        if (changes.size() > static_cast<size_t>(rsi_period + 1)) {
            changes.erase(changes.begin(), changes.end() - (rsi_period + 1));
        }

        double avg_gain = 0.0;
        double avg_loss = 0.0;

        // Первоначальный расчет средних gain/loss
        for (int i = 0; i < rsi_period; ++i) {
            if (changes[i] > 0) avg_gain += changes[i];
            else avg_loss += std::abs(changes[i]);
        }

        avg_gain /= rsi_period;
        avg_loss /= rsi_period;

        // Расчет RSI по формуле Уайлдера (начинаем с rsi_period, так как первые rsi_period точек уже использованы)
        for (size_t i = rsi_period; i < changes.size(); ++i) {
            double current_change = changes[i];
            double gain = 0.0, loss = 0.0;

            if (current_change > 0) gain = current_change;
            else loss = std::abs(current_change);

            // Сглаживание средних значений
            avg_gain = (avg_gain * (rsi_period - 1) + gain) / rsi_period;
            avg_loss = (avg_loss * (rsi_period - 1) + loss) / rsi_period;
        }

        if (avg_loss == 0.0) return 100.0;
        double rs = avg_gain / avg_loss;
        return 100.0 - (100.0 / (1.0 + rs));
    }
};

#endif