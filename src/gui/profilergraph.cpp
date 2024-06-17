/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2018 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "profilergraph.h"
#include "porting.h"
#include "util/string.h"

void ProfilerGraph::put(const Profiler::GraphValues &values)
{
	for (auto& graph : m_log2) {
		const auto iter_for_value = values.find(graph.first);
		auto& queue = graph.second.queue;
		if (iter_for_value == values.end()) {
			queue.emplace_back(NAN);
			// Remove entry if it contains only NaNs.
			bool empty = true;
			for (const auto value : queue) {
				if (value != NAN) {
					empty = false;
					break;
				}
			}
			if (empty) {
				m_log2.erase(graph.first);
				continue;
			}
		}
		else {
			const auto value = iter_for_value->second;
			queue.emplace_back(value);
			if (value > graph.second.max)
				graph.second.max = value;
			if (value == value && value < graph.second.min)
				graph.second.min = value;
		}
		// Earse values beyond limit
		while (queue.size() > m_log_max_size) {
			const auto begin = queue.begin();
			const auto value_at_begin = queue[0];
			queue.erase(begin);
			if (value_at_begin == graph.second.max) {
				// Find new max
				graph.second.max = 0;
				for (const auto value : queue)
					if (value == value && value > graph.second.max)
						graph.second.max = value;
			}
			if (value_at_begin == graph.second.min) {
				// Find new min
				graph.second.min = graph.second.max;
				for (const auto value : queue)
					if (value == value && value < graph.second.min)
						graph.second.min = value;
			}
		}
	}

	// Add new graphs
	for (const auto& value : values) {
		const auto iter_for_log = m_log2.find(value.first);
		if (iter_for_log == m_log2.end()) {
			auto& new_graph = m_log2[value.first];
			new_graph.max = value.second;
			new_graph.min = value.second;
			new_graph.queue = std::deque<float>((size_t)(m_log_max_size - 1), NAN);
			new_graph.queue.emplace_back(value.second);
		}
	}
}

void ProfilerGraph::draw(s32 x_left, s32 y_bottom, video::IVideoDriver *driver,
		gui::IGUIFont *font) const
{
	// Assign colors
	static const video::SColor usable_colors[] = {video::SColor(255, 255, 100, 100),
			video::SColor(255, 90, 225, 90),
			video::SColor(255, 100, 100, 255),
			video::SColor(255, 255, 150, 50),
			video::SColor(255, 220, 220, 100)};
	static const u32 usable_colors_count =
			sizeof(usable_colors) / sizeof(*usable_colors);
	s32 graphh = 50;
	s32 textx = x_left + m_log_max_size + 15;
	s32 textx2 = textx + 200 - 15;
	s32 graph_i = 0;

	for (const auto &graph : m_log2) {
		const std::string &id = graph.first;
		video::SColor color = graph_i <= usable_colors_count ? usable_colors[graph_i] : video::SColor(255, 200, 200, 200);
		s32 x = x_left;
		s32 y = y_bottom - graph_i * 50;
		float show_min = graph.second.min;
		float show_max = graph.second.max;

		if (show_min >= -0.0001 && show_max >= -0.0001) {
			if (show_min <= show_max * 0.5)
				show_min = show_min;
		}

		// Text drawing

		const s32 texth = 15;
		char buf[32];

		// Graph name
		font->draw(utf8_to_wide(id).c_str(),
				core::rect<s32>(textx, y - graphh / 2 - texth / 2, textx2,
						y - graphh / 2 + texth / 2),
				color);

		// Graph border values
		static const char formats_by_precision[2][5] = { "%.3g", "%.5g" };
		char format[16];
		porting::mt_snprintf(format, sizeof(format),
				"%s\n\n%s",
				formats_by_precision[floorf(show_max) == show_max],
				formats_by_precision[floorf(show_min) == show_min]);
		porting::mt_snprintf(buf, sizeof(buf),
				format, show_max, show_min);
		font->draw(buf,
				core::rect<s32>(textx, y - graphh, textx2,
					y - graphh + texth),
				color);

		// Graph drawing

		s32 graph1y = y;
		s32 graph1h = graphh;
		bool relativegraph = (show_min != 0 && show_min != show_max);
		float lastscaledvalue = 0.0;
		bool lastscaledvalue_exists = false;

		//auto sp3 = ScopeProfiler(g_profiler, "ProfilerGraph::draw() draw new");

		//new
		for (const auto& value : graph.second.queue) {

			bool value_exists = value == value; // False if value is NaN
			if (!value_exists) {
				x++;
				lastscaledvalue_exists = false;
				continue;
			}

			float scaledvalue = 1.0;

			if (show_max != show_min)
				scaledvalue = (value - show_min) / (show_max - show_min);

			if (scaledvalue == 1.0 && value == 0) {
				x++;
				lastscaledvalue_exists = false;
				continue;
			}

			if (relativegraph) {
				if (lastscaledvalue_exists) {
					s32 ivalue1 = lastscaledvalue * graph1h;
					s32 ivalue2 = scaledvalue * graph1h;
					driver->draw2DLine(
						v2s32(x - 1, graph1y - ivalue1),
						v2s32(x, graph1y - ivalue2),
						color);
				}

				lastscaledvalue = scaledvalue;
				lastscaledvalue_exists = true;
			}
			else {
				s32 ivalue = scaledvalue * graph1h;
				driver->draw2DLine(v2s32(x, graph1y),
					v2s32(x, graph1y - ivalue), color);
			}

			x++;
		}
		graph_i++;
	}
}
