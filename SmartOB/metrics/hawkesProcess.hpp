#include <cmath>
#include <cstdlib>
#include <limits>
#include <numbers>
#include <numeric>
#include <span>
#include <tuple>
#include <vector>
#include "../../features/data/ticks.hpp"
#pragma once

namespace hawkesProcess{
	constexpr double msToSeconds(std::int64_t dt_ms) {
	    return static_cast<double>(dt_ms) * 1e-3;
	}

	double L(std::span<Trade> ticks, double alpha, double beta, double mu){

		if (ticks.size() < 2 ||
		    !std::isfinite(alpha) ||
		    !std::isfinite(beta) ||
		    !std::isfinite(mu) ||
		    alpha <= 0.0 ||
		    alpha >= 1.0 ||
		    beta <= 0.0 ||
		    mu <= 0.0) {
		    return std::numeric_limits<double>::quiet_NaN();
		}

		std::vector<double> A(ticks.size());
		std::vector<double> lambda(ticks.size());
		A[0] = 0;
		for (int i{1}; i < A.size(); i++){
			A[i] = std::pow(std::numbers::e, -beta * msToSeconds(ticks[i].ts - ticks[i-1].ts)) * (1 + A[i-1]);
		}
		
		for (int i{}; i < lambda.size(); i++){
			lambda[i] = mu + alpha * beta * A[i];
		}
		
		double s1 = std::accumulate(lambda.begin(), lambda.end(), 0.0, [](auto init, auto first){return init + std::log(first);});
		
		double tsSum = std::accumulate(ticks.begin(), ticks.end(), 0.0, [&](auto init, Trade first){
				return init + (1 - std::pow(std::numbers::e, -beta * msToSeconds(ticks.back().ts - first.ts)));
		});

		double s2 = mu * msToSeconds(ticks.back().ts - ticks.front().ts) + alpha * tsSum;
		
		
		if (ticks.size() <= 0) return std::numeric_limits<double>::quiet_NaN();
		
		return (s1 - s2) / static_cast<double>(ticks.size());
	}


	auto optimizer(double alpha, double beta, double mu, std::span<Trade> ticks){
		double epsilon = 1e-4, h = 1e-5;
		double p_a, p_b, p_m;

		auto slope = [&](double a, double b, double m){
			double L_0 = L(ticks, a, b, m);

			double s_a = (L(ticks, a+h, b, m) - L_0) / h;
			double s_b = (L(ticks, a, b+h, m) - L_0) / h;
			double s_m = (L(ticks, a, b, m+h) - L_0) / h;
			return std::make_tuple(s_a, s_b, s_m);
		};

		std::size_t i{};
		
		double curr = L(ticks, alpha, beta, mu);
		do {
			if (i > 5000) break;
			std::tie(p_a, p_b, p_m) = slope(alpha, beta, mu);

			double step = 1.0;
			while (step > 1e-12 && !(L(ticks, alpha + step*p_a, beta + step*p_b, mu + step*p_m) > curr))
				step *= 0.5;

			if (step <= 1e-12) break;

			alpha = alpha + step*p_a;
			beta = beta + step*p_b;
			mu = mu + step*p_m;

			curr = L(ticks, alpha, beta, mu);
			i++;
		} while (std::abs(p_a) > epsilon || std::abs(p_b) > epsilon || std::abs(p_m) > epsilon);

		return std::make_tuple(alpha, beta, mu);
	}



	std::pair<double, double> generate(std::span<Trade> ticks){
		double alpha = 0.5, beta = 1, mu = (1 - alpha);
		std::tie(alpha, beta, mu) = optimizer(alpha, beta, mu, ticks);
		
		std::vector<double> data_(ticks.size());	
		auto phi = [&](double num){
			return num >= 0 ? alpha * beta * std::pow(std::numbers::e, -beta * num) : 0;
		};

		for (int i{}; i < data_.size(); i++){
			data_[i] = mu;
			for (int j{}; j < i; j++)
				data_[i] +=  phi(msToSeconds(ticks[i].ts - ticks[j].ts));
		}

		return {data_.back(), alpha};
	}


}
