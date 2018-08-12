#pragma once
#include "LogicGraph.h"
#include <tbb\tbb.h>

namespace ESL
{
	class TbbGraph
	{
		using LogicNode = tbb::flow::continue_node<tbb::flow::continue_msg>;
		using StartNode = tbb::flow::broadcast_node<tbb::flow::continue_msg>;
		tbb::flow::graph _flowGraph;
		StartNode _startNode;
		std::vector<LogicNode> _logicNodes;

		friend class LogicGraph;

	public:
		TbbGraph() : _flowGraph(), _startNode(_flowGraph) {}
		void RunOnce()
		{
			_startNode.try_put(tbb::flow::continue_msg{});
			_flowGraph.wait_for_all();
		}
		~TbbGraph()
		{

		}
	};

	template<>
	void LogicGraph::BuildGraph<TbbGraph>(TbbGraph& graph)
	{
		std::size_t size = _flattenNodes.size();
		lni::vector<TbbGraph::LogicNode*> nodes;
		graph._logicNodes.reserve(size + 1);
		for (long long i = size - 1; i >= 0; i--)
		{
			auto& node = _flattenNodes[i];
			graph._logicNodes.emplace_back(graph._flowGraph, [f = node->task](tbb::flow::continue_msg) { f(); });
			nodes[node->id] = &graph._logicNodes[size - i - 1];
			for (auto &succ : node->successors)
				tbb::flow::make_edge(*nodes[node->id], *nodes[succ->id]);
		}
		for (auto &node : _entry)
		{
			tbb::flow::make_edge(graph._startNode, *nodes[node->id]);
		}
	}
}