#ifndef ___NETWORK_CONDITIONER_H___
#define ___NETWORK_CONDITIONER_H___

#include <iostream>
#include <random>
#include <chrono>
#include <string>

enum class LossRateType {
    LOSS_10_PERCENT,  ///< 10%
    LOSS_20_PERCENT,  ///< 20%
    LOSS_30_PERCENT,  ///< 30%
    LOSS_40_PERCENT,  ///< 40%
    LOSS_50_PERCENT   ///< 50%
};

class RandomLossTool {
private:
    double loss_rate;
    std::default_random_engine engine;
    std::uniform_real_distribution<double> dist;

    void set_loss_rate(LossRateType type) {
        switch (type) {
            case LossRateType::LOSS_10_PERCENT: loss_rate = 0.1; break;
            case LossRateType::LOSS_20_PERCENT: loss_rate = 0.2; break;
            case LossRateType::LOSS_30_PERCENT: loss_rate = 0.3; break;
            case LossRateType::LOSS_40_PERCENT: loss_rate = 0.4; break;
            case LossRateType::LOSS_50_PERCENT: loss_rate = 0.5; break;
            default: loss_rate = 0.0;
        }
    }

public:
    RandomLossTool(LossRateType target_loss) : dist(0.0, 1.0) {
        unsigned int seed = std::chrono::system_clock::now().time_since_epoch().count();
        engine.seed(seed);
        set_loss_rate(target_loss);
    }

    void switch_loss_rate(LossRateType new_loss) {
        set_loss_rate(new_loss);
        std::cout << "switched to ";
        if (new_loss == LossRateType::LOSS_10_PERCENT) std::cout << "10%";
        else if (new_loss == LossRateType::LOSS_20_PERCENT) std::cout << "20%";
        else if (new_loss == LossRateType::LOSS_30_PERCENT) std::cout << "30%";
        else if (new_loss == LossRateType::LOSS_40_PERCENT) std::cout << "40%";
        else if (new_loss == LossRateType::LOSS_50_PERCENT) std::cout << "50%";
        std::cout << " random loss\n";
    }

    bool is_packet_lost() {
        return dist(engine) < loss_rate;
    }

    std::string get_current_loss_desc() {
        return std::to_string(loss_rate * 100) + "% lossrate";
    }
};

class GilbertElliottLossTool {
private:
    double p;            ///< good -> bad probability 
    double r;            ///< bad -> good probability 
    double g;            ///< lossrate on good
    double b;            ///< lossrate on bad
    bool current_state;  ///< current state: true=good, false=bad
    std::default_random_engine engine;
    std::uniform_real_distribution<double> dist;

    void load_params(LossRateType type) {
        switch (type) {
        case LossRateType::LOSS_10_PERCENT:
            p = 0.04; r = 0.2; g = 0.02; b = 0.55;  ///< ~10%
            break;
        case LossRateType::LOSS_20_PERCENT:
            p = 0.05; r = 0.15; g = 0.03; b = 0.65; ///< ~20%
            break;
        case LossRateType::LOSS_30_PERCENT:
            p = 0.06; r = 0.1; g = 0.02; b = 0.75;  ///< ~30%
            break;
        case LossRateType::LOSS_40_PERCENT:
            p = 0.09; r = 0.09; g = 0.05; b = 0.82; ///< ~40%
            break;
        case LossRateType::LOSS_50_PERCENT:
            p = 0.12; r = 0.07; g = 0.1; b = 0.9;   ///< ~50%
            break;
        }
        current_state = true;
    }

public:
    GilbertElliottLossTool(LossRateType target_loss) : dist(0.0, 1.0) {
        unsigned int seed = std::chrono::system_clock::now().time_since_epoch().count();
        engine.seed(seed);
        load_params(target_loss);
    }

    void switch_lossrate(LossRateType new_loss) {
        load_params(new_loss);
        std::cout << "switched to ";
        if (new_loss == LossRateType::LOSS_10_PERCENT)      std::cout << "10%";
        else if (new_loss == LossRateType::LOSS_30_PERCENT) std::cout << "30%";
        else if (new_loss == LossRateType::LOSS_40_PERCENT) std::cout << "40%";
        else if (new_loss == LossRateType::LOSS_50_PERCENT) std::cout << "50%";
        std::cout << " lossrate scene\n";
    }

    bool is_packet_lost() {
        double rand_val = dist(engine);
        if (current_state) {
            if (rand_val < p) current_state = false;  ///< good -> bad
        } else {
            if (rand_val < r) current_state = true;   ///< bad -> good
        }

        rand_val = dist(engine);
        return current_state ? (rand_val < g) : (rand_val < b);
    }

    std::string get_current_loss_desc() {
        if (p == 0.04 && r == 0.2) return "10% lossrate(Wi-Fi/4G)";
        else if (p == 0.05 && r == 0.15) return "20% lossrate(weak Wi-Fi/cellular)";
        else if (p == 0.06 && r == 0.1)  return "30% lossrate(weak Wi-Fi/cellular )";
        else if (p == 0.09 && r == 0.09) return "40% lossrate(public Wi-Fi with high traffic)";
        else if (p == 0.12 && r == 0.07) return "50% lossrate(3G/severe interference)";
        return "unknown lossrate scene";
    }
};

#endif ///< ___NETWORK_CONDITIONER_H___
