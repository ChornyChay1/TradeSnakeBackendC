#include <iostream>
#include <map>
#include <optional>
#ifndef STRUCT_HPP
#define STRUCT_HPP


struct HistoricalData {
    std::string timestamp;
    double open;
    double close;
    double high;
    double low;
    double volume;
    double turnover;
};

struct HistoricalResult {
    std::string timestamp;
    double open;
    double close;
    double high;
    double low;
    double volume;
    double turnover;
    std::map<std::string, double> buy;   
    std::map<std::string, double> sell; 
};


struct CandleData {
    std::string timestamp;
    double open;           
    double close;          
    double high;           
    double low;            
    double volume;         
    double turnover;       
};
#endif