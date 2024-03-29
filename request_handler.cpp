#include "request_handler.h"

#include <unordered_set>
#include <vector>
#include <utility>
#include <functional>

namespace request_handler {
	using namespace domain;

	RequestHandler::RequestHandler(transport::TransportCatalogue& db, renderer::MapRenderer& mr)
		: db_(db)
		, mr_(mr)
	{}

	void RequestHandler::AddBus(const std::string_view raw_query) {
		auto [words, separator] = SplitIntoWordsBySeparator(raw_query);
		auto [route, unique_stops] = WordsToRoute(words, separator);
		const auto [geographic, actual] = ComputeRouteLengths(route);
		Bus new_bus(
			std::move(words[0]), 
			std::move(StopsToStopPtr(std::move(route))), 
			unique_stops, 
			actual, 
			geographic
		);

		db_.AddBus(std::move(new_bus));
	}

	void RequestHandler::AddBus(Bus&& bus) {
		db_.AddBus(std::move(bus));
	}

	void RequestHandler::AddStop(const std::string_view raw_query) {
		auto [words, _] = SplitIntoWordsBySeparator(raw_query);
		Stop new_stop(
			std::move(words[0]), 
			std::stod(words[1]), 
			std::stod(words[2])
		);

		db_.AddStop(std::move(new_stop));
	}

	void RequestHandler::AddStop(Stop&& stop) {
		db_.AddStop(std::move(stop));
	}

	void RequestHandler::SetDistanceBetweenStops(const std::string_view raw_query) {
		auto [parts, _] = SplitIntoWordsBySeparator(raw_query);
		const auto& stop_X = parts[0];
		for (size_t i = 3; i < parts.size(); ++i) {
			auto [raw_distance, stop_To] = SplitIntoLengthStop(move(parts[i]));
			int distance = std::stoi(move(raw_distance.substr(0, raw_distance.size() - 1)));

			db_.SetDistanceBetweenStops(stop_X, stop_To, distance);
		}
	}

	void RequestHandler::SetDistanceBetweenStops(const std::string_view first, const std::string_view second, int distance) {
		db_.SetDistanceBetweenStops(first, second, distance);
	}

	BusPtr RequestHandler::SearchBus(const std::string_view name) const {
		return db_.SearchBus(name);
	}

	StopPtr RequestHandler::SearchStop(const std::string_view name) const {
		return db_.SearchStop(name);
	}

	const std::vector<BusPtr> RequestHandler::GetBusesInVector() const {
		return db_.GetBusesInVector();
	}

	const std::vector<StopPtr> RequestHandler::GetStopsInVector() const {
		return db_.GetStopsInVector();
	}

	std::optional<BusStat> RequestHandler::GetBusStat(const std::string_view bus_name) const {
		BusPtr bus = db_.SearchBus(bus_name);
		if (bus == nullptr) {
			return {};
		}

		return std::optional<BusStat>({
			bus_name,
			static_cast<int>(bus.get()->route.size()),
			bus->unique_stops,
			bus->route_actual_length,
			bus->route_actual_length / bus->route_geographic_length
		});
	}

	std::optional<StopStat> RequestHandler::GetStopStat(const std::string_view stop_name) const {
		StopPtr stop = db_.SearchStop(stop_name);
		if (stop == nullptr) {
			return {};
		}

		return std::optional<StopStat>({
			stop_name,
			GetBusesByStop(stop_name)
		});
	}

	const std::unordered_set<BusPtr>* RequestHandler::GetBusesByStop(const std::string_view stop_name) const {
		StopPtr stop = db_.SearchStop(stop_name);

		return db_.GetPassingBusesByStop(stop);
	}

	std::tuple<double, int> RequestHandler::ComputeRouteLengths(const std::vector<std::string_view>& route) const {
		double geographic = 0;
		int actual = 0;

		auto prev_stop = &route[0];
		size_t route_sz = route.size();
		for (size_t i = 1; i < route_sz; ++i) {
			const auto cur_stop = &route[i];
			const auto res_geogr = db_.GetGeographicDistanceBetweenStops(*prev_stop, *cur_stop);
			geographic += (res_geogr.has_value()) ? *res_geogr : 0;

			const auto res_actual = db_.GetActualDistanceBetweenStops(*prev_stop, *cur_stop);
			actual += (res_actual.has_value()) ? *res_actual : 0;

			prev_stop = cur_stop;
		}

		return std::tuple<double, int>(geographic, actual);
	}

	std::vector<StopPtr> RequestHandler::StopsToStopPtr(const std::vector<std::string_view>& stops) const {
		std::vector<StopPtr> result;
		result.reserve(stops.size());
		for (const auto& stop : stops) {
			result.push_back(db_.SearchStop(stop));
		}

		return result;
	}

	std::optional<int> RequestHandler::GetActualDistanceBetweenStops(const std::string_view stop1_name, const std::string_view stop2_name) const {
		return db_.GetActualDistanceBetweenStops(stop1_name, stop2_name);
	}

	svg::Document RequestHandler::RenderMap() const {
		std::vector<BusPtr> buses = db_.GetBusesInVector();

		std::vector<std::pair<StopPtr, StopStat>> stops;
		for (StopPtr stop : db_.GetStopsInVector()) {
			stops.emplace_back(std::pair<StopPtr, StopStat>{ stop, *GetStopStat(*stop.get()->name.get()) });
		}

		return mr_.MakeDocument(std::move(buses), std::move(stops));
	}

	void RequestHandler::SetRenderSettings(renderer::RenderingSettings&& settings) {
		mr_.SetSettings(std::move(settings));
	}

	void RequestHandler::SetRoutingSettings(const double bus_wait_time, const double bus_velocity) {
		rt_.SetSettings(bus_wait_time, bus_velocity);
	}

	void RequestHandler::AddStopToRouter(const std::string_view name) {
		rt_.AddStop(name);
	}

	void RequestHandler::AddWaitEdgeToRouter(const std::string_view stop_name) {
		rt_.AddWaitEdge(stop_name);
	}

	void RequestHandler::AddBusEdgeToRouter(const std::string_view stop_from, const std::string_view stop_to, const std::string_view bus_name, const int span_count, const int dist) {
		rt_.AddBusEdge(stop_from, stop_to, bus_name, span_count, dist);
	}

	void RequestHandler::BuildRouter() {
		rt_.BuildGraph();
		rt_.BuildRouter();
	}

	std::optional<transport::RouteInfo> RequestHandler::GetRouteInfo(const std::string_view from, const std::string_view to) const {
		return rt_.GetRouteInfo(from, to);
	}

	std::tuple<std::string, size_t> RequestHandler::QueryGetName(const std::string_view str) const {
		auto pos = str.find_first_of(' ', 0) + 1;
		auto new_pos = str.find_first_of(':', pos);
		if (new_pos == str.npos) {
			return std::tuple<std::string, size_t>(std::move(str.substr(pos)), new_pos);
		}
		auto name = str.substr(pos, new_pos - pos);

		return std::tuple<std::string, size_t>(std::move(name), ++new_pos);
	}

	std::tuple<std::string, std::string> RequestHandler::SplitIntoLengthStop(std::string&& str) const {
		const auto pos = str.find_first_of(' ');
		std::string length		= str.substr(0, pos);
		std::string name_stop	= str.substr(pos + 4);

		return { std::move(length), std::move(name_stop) };
	}

	std::tuple<std::vector<std::string>, RequestHandler::SeparatorType> RequestHandler::SplitIntoWordsBySeparator(const std::string_view str) const {
		std::vector<std::string> words;
		SeparatorType sep_type;

		auto [name, pos_start] = QueryGetName(str);
		words.push_back(std::move(name));

		const size_t str_sz = str.size();
		std::string word;
		for (size_t i = pos_start; i < str_sz; ++i) {
			if (str[0] == 'B' && (str[i] == '>' || str[i] == '-')) {
				sep_type = (str[i] == '>') ? SeparatorType::GREATER_THAN : SeparatorType::DASH;
				words.push_back(std::move(word.substr(1, word.size() - 2)));
				word.clear();
			} else if (str[0] == 'S' && str[i] == ',') {
				words.push_back(std::move(word.substr(1, word.size() - 1)));
				word.clear();
			} else {
				word += str[i];
			}
		}
		words.push_back(std::move(word.substr(1, word.size() - 1)));

		return std::tuple<std::vector<std::string>, SeparatorType>(std::move(words), sep_type);
	}

	std::tuple<std::vector<std::string_view>, int> RequestHandler::WordsToRoute(const std::vector<std::string>& words, SeparatorType separator) const {
		std::vector<std::string_view> result;
		std::unordered_set<std::string_view, std::hash<std::string_view>> stops_unique_names;
		result.reserve(words.size() - 1);

		for (size_t i = 1; i < words.size(); ++i) {
			StopPtr stop = db_.SearchStop(words[i]);
			result.push_back(*stop->name.get());
			stops_unique_names.insert(words[i]);
		}

		if (separator == SeparatorType::DASH) {
			result.reserve(words.size() * 2 - 1);
			for (int i = words.size() - 2; i >= 1; --i) {
				StopPtr stop = db_.SearchStop(words[i]);
				result.push_back(*stop->name.get());
			}
		}

		return {
			std::move(result),
			(int)stops_unique_names.size()
		};
	}

}