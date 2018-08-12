#pragma once
#include "LogicGraph.h"
#include <sstream>
#include <fstream>

namespace ESL
{
	class VisualGraph
	{
		std::string graphvizCode;
		friend class LogicGraph;
	public:
		void SaveTo(std::string url)
		{
			std::ofstream stream(url);
			stream << graphvizCode;
		}
	};

	template<>
	void LogicGraph::BuildGraph<VisualGraph>(VisualGraph& graph)
	{
		std::stringstream stream{ graph.graphvizCode };
		stream << "digraph framegraph \n{\n";

		stream << "rankdir = LR\n";
		stream << "bgcolor = black\n\n";
		stream << "node [shape=rectangle, fontname=\"helvetica\", fontsize=12, fontcolor=white]\n\n";

		for (auto& node : _graph)
		{
			stream << "\"" << node->name << "\" [label=\"" << node->name << "\", style=bold, color=darkorange]\n";
		}

		stream << "\n";

		for (auto& node : _graph)
		{
			for (auto& l : node->successors)
			{
				stream << "\"" << node->name << "\" -> { ";
				stream << "\"" << l->name << "\" ";
				stream << "} [color=white]\n";
			}
		}

		stream << "\n";
		stream << "}";
	}
}