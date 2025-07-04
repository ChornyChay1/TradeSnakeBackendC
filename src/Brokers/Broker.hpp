#ifndef BROKER_HPP
#define BROKER_HPP

#include <iostream>
#include <string>
#include <memory>
#include <mysql/jdbc.h>
#include "./const.hpp"

class Broker {
private:
    double spred;
    double procent_comission;
    double fix_comission;
    int broker_id;
    std::shared_ptr<sql::Connection> con;

    // Метод для заполнения данных о брокере из базы данных
    void fetchBrokerData() {
        if (!con) {
            std::cerr << "Database connection is not established!" << std::endl;
            return;
        }

        try {
            std::shared_ptr<sql::PreparedStatement> pstmt(
                con->prepareStatement("SELECT spred, procent_comission, fox_comission FROM brokers WHERE id = ?")
            );
            pstmt->setInt(1, broker_id);

            std::shared_ptr<sql::ResultSet> res(pstmt->executeQuery());

            if (res->next()) {
                spred = res->getDouble("spred");
                procent_comission = res->getDouble("procent_comission");
                fix_comission = res->getDouble("fox_comission"); 
            }
            else {
                std::cerr << "Broker with id " << broker_id << " not found!" << std::endl;
            }
        }
        catch (const sql::SQLException& e) {
            std::cerr << "SQL Error: " << e.what() << std::endl;
        }
    }



public:
    Broker(int broker_id)
        : broker_id(broker_id), spred(0), procent_comission(0), fix_comission(0) {
        con = Constants::createConnection();
        fetchBrokerData();  
    }
    double calculateRealPriceSell(double current_price,double quantity) {
        double real_price = (current_price * quantity - spred - (procent_comission / 100.0 * current_price * quantity) - fix_comission)/quantity;
        return real_price;
    }
    double calculateRealPriceBuy(double current_price, double quantity) {
        double real_price = (current_price * quantity + spred + (procent_comission / 100.0 * current_price * quantity) + fix_comission) / quantity;
        return real_price;
    }
    void sell(int bot_id, double current_price,double real_price, double quantity) {
        std::shared_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("INSERT INTO trades (bot_id, type_id, price, price_by_broker, quantity, time) VALUES (?, ?, ?, ?, ?, NOW())")
        );
        try {
            hold(bot_id, real_price);
            pstmt->setInt(1, bot_id);
            pstmt->setInt(2, 2);  
            pstmt->setDouble(3, current_price* quantity);
            pstmt->setDouble(4, real_price * quantity);
            pstmt->setDouble(5, quantity);

            pstmt->executeUpdate();

            // Обновляем информацию о боте
            std::shared_ptr<sql::PreparedStatement> update_pstmt(
                con->prepareStatement("UPDATE bots SET money = money + ?, symbol_count = 0 WHERE id = ?")
            );
            update_pstmt->setDouble(1, quantity * current_price); // Добавляем деньги
            update_pstmt->setInt(2, bot_id);  // Обновляем для текущего бота
            update_pstmt->executeUpdate();
        }
        catch (sql::SQLException& e) {
            std::cerr << "Error during SELL operation: " << e.what() << std::endl;
        }
    }
    void hold(int bot_id, double current_price) {
        try {
            std::shared_ptr<sql::PreparedStatement> pstmt(
                con->prepareStatement("UPDATE bots SET current_price = ? WHERE id = ?")
            );
            pstmt->setDouble(1, current_price);
            pstmt->setInt(2, bot_id);
            pstmt->executeUpdate();
        }
        catch (sql::SQLException& e) {
            std::cerr << "Error during HOLD operation: " << e.what() << std::endl;
        }
    }

    void buy(int bot_id, double current_price, double real_price, double quantity) {
        std::shared_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("INSERT INTO trades (bot_id, type_id, price, price_by_broker, quantity, time) VALUES (?, ?, ?, ?, ?, NOW())")
        );
        try {
            hold(bot_id, real_price);
            pstmt->setInt(1, bot_id);
            pstmt->setInt(2, 1); 
            pstmt->setDouble(3, current_price*quantity);
            pstmt->setDouble(4, real_price * quantity);
            pstmt->setDouble(5, quantity);

            pstmt->executeUpdate();

            // Обновляем информацию о боте
            std::shared_ptr<sql::PreparedStatement> update_pstmt(
                con->prepareStatement("UPDATE bots SET money = money - ?, symbol_count = symbol_count + ? WHERE id = ?")
            );
            update_pstmt->setDouble(1, quantity * current_price); // Снимаем деньги
            update_pstmt->setDouble(2, quantity); // Увеличиваем количество символов
            update_pstmt->setInt(3, bot_id);  // Обновляем для текущего бота
            update_pstmt->executeUpdate();
        }
        catch (sql::SQLException& e) {
            std::cerr << "Error during BUY operation: " << e.what() << std::endl;
        }
    }

    // Геттеры для доступа к данным брокера
    double getSpred() const { return spred; }
    double getProcentComission() const { return procent_comission; }
    double getFixComission() const { return fix_comission; }
};

#endif