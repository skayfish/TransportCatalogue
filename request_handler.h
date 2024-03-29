#pragma once

#include "transport_catalogue.h"
#include "map_renderer.h"
#include "transport_router.h"

#include <optional>
#include <unordered_set>
#include <tuple>
#include <string>
#include <string_view>
#include <memory>

namespace request_handler {

	class RequestHandler {
	private:
		enum class SeparatorType {
			DASH,
			GREATER_THAN,
			NO_HAVE
		};

	public:
		RequestHandler(transport::TransportCatalogue& db, renderer::MapRenderer& mr);

		void AddBus(const std::string_view raw_query);
		void AddBus(domain::Bus&& bus);
		void AddStop(const std::string_view raw_query);
		void AddStop(domain::Stop&& stop);

		void SetDistanceBetweenStops(const std::string_view raw_query);
		void SetDistanceBetweenStops(const std::string_view first, const std::string_view second, int distance);

		domain::BusPtr  SearchBus(const std::string_view name)  const;
		domain::StopPtr SearchStop(const std::string_view name) const;

		const std::vector<domain::BusPtr>  GetBusesInVector() const;
		const std::vector<domain::StopPtr> GetStopsInVector() const;

		std::optional<domain::BusStat>  GetBusStat(const std::string_view bus_name)   const;
		std::optional<domain::StopStat> GetStopStat(const std::string_view stop_name) const;

		const std::unordered_set<domain::BusPtr>* GetBusesByStop(const std::string_view stop_name)                const;
		std::tuple<double, int>                   ComputeRouteLengths(const std::vector<std::string_view>& routh) const;
		std::vector<domain::StopPtr>              StopsToStopPtr(const std::vector<std::string_view>& stops)      const;

		std::optional<int>                        GetActualDistanceBetweenStops(const std::string_view stop1_name, const std::string_view stop2_name) const;

		svg::Document RenderMap() const;
		void SetRenderSettings(renderer::RenderingSettings&& settings);

		void SetRoutingSettings(const double bus_wait_time, const double bus_velocity);
		void AddStopToRouter(const std::string_view name);
		void AddWaitEdgeToRouter(const std::string_view stop_name);
		void AddBusEdgeToRouter(
			const std::string_view stop_from, 
			const std::string_view stop_to, 
			const std::string_view bus_name, 
			const int span_count, 
			const int dist
		);
		void BuildRouter();
		std::optional<transport::RouteInfo> GetRouteInfo(const std::string_view from, const std::string_view to) const;

	private:
		transport::TransportCatalogue& db_;
		renderer::MapRenderer&         mr_;
		transport::Router              rt_;

		std::tuple<std::string, std::size_t>                QueryGetName(const std::string_view str)                                     const;
		std::tuple<std::string, std::string>                SplitIntoLengthStop(std::string&& str)                                       const;
		std::tuple<std::vector<std::string>, SeparatorType> SplitIntoWordsBySeparator(const std::string_view str)                        const;
		std::tuple<std::vector<std::string_view>, int>      WordsToRoute(const std::vector<std::string>& words, SeparatorType separator) const;
	};
}
