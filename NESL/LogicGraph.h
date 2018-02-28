#pragma once
#include "States.h"
#include "MPL.h"
#include "Dispather.h"
#include <unordered_set>
#include <queue>
#include "small_vector.h"
#include <iostream>
#include <tbb\tbb.h>


namespace ESL
{
	class LogicGraph
	{
		using LogicNode = tbb::flow::continue_node<tbb::flow::continue_msg>;
		using StartNode = tbb::flow::broadcast_node<tbb::flow::continue_msg>;
		tbb::flow::graph _logicGraph;
		StartNode _startNode;
		std::vector<LogicNode> _logicNodes;

		friend class LogicGraphBuilder;

	public:
		LogicGraph() : _logicGraph(), _startNode(_logicGraph) {}
		void Flow()
		{
			_startNode.try_put(tbb::flow::continue_msg{});
			_logicGraph.wait_for_all();
		}
		~LogicGraph()
		{

		}
	};


	class LogicGraphBuilder
	{
		class LogicNode
		{
			std::function<void(void)> _dispatcher;
			chobo::small_vector<std::size_t, 8> _reads;
			chobo::small_vector<std::size_t, 8> _writes;
			chobo::small_vector<LogicNode*, 8> _successors;
			tbb::flow::continue_node<tbb::flow::continue_msg>* _graphNode;
			std::string _name;
			int _refcount = 0;
			friend LogicGraphBuilder;
		public:
			chobo::small_vector<std::string, 8> dependencies;
		};
		std::unordered_map<std::string, LogicNode> _nodes;
		lni::vector<LogicNode*> _flattenNodes;
		chobo::small_vector<LogicNode*> _entry;
		States &states;

		void CalculateSuccessor()
		{
			for (auto &i : _nodes)
			{
				auto &node = i.second;
				if (node.dependencies.empty())
				{
					_entry.push_back(&node);
					continue;
				}
				for (auto& dn : node.dependencies)
				{
					auto it = _nodes.find(dn);
					if (it == _nodes.end()) abort();
					it->second._successors.push_back(&node);
					node._refcount += 1;
				}
			}
		}

		void AddSuccessor(LogicNode* node, LogicNode* succ)
		{
			auto &succs = node->_successors;
			if (std::find(succs.begin(), succs.end(), succ) == succs.end())
			{
				std::cerr << "Implicit dependency between " << succ->_name.c_str() << " and " << node->_name.c_str() << " due to conflict.\n";
				succs.push_back(succ);
			}
		}

		void ImplicitDependency()
		{
			std::queue<LogicNode*> searching;
			std::unordered_map<std::size_t, lni::vector<LogicNode*>> readers;
			std::unordered_map<std::size_t, LogicNode*> writers;

			for (auto &node : _entry)
				searching.push(node);

			_flattenNodes.reserve(_nodes.size());
			while (!searching.empty())
			{
				LogicNode* node = searching.front();

				_flattenNodes.push_back(node);

				searching.pop();
				for (auto &succ : node->_successors)
				{
					succ->_refcount -= 1;
					if (succ->_refcount <= 0)
						searching.push(succ);
				}

				for (auto &read : node->_reads)
				{
					readers[read].push_back(node);
					if (auto it = writers.find(read); it != writers.end())
						AddSuccessor(it->second, node);
				}

				for (auto &write : node->_writes)
				{
					if (auto it = readers.find(write); it != readers.end())
					{
						for (auto &reader : it->second)
							AddSuccessor(reader, node);
						it->second.clear();
					}
						

					if (auto it = writers.find(write); it != writers.end())
						AddSuccessor(it->second, node);
					writers[write] = node;
				}
			}
		}

		void LoopDependency()
		{
			for (auto& node : _nodes)
			{
				if (node.second._refcount > 0)
				{
					std::cerr << "Loop dependency with " << node.second._name.c_str() << ".\n";
				}
			}
		}

		void BuildSuccessor()
		{
			CalculateSuccessor();
			ImplicitDependency();
			LoopDependency();
		}


	public:
		LogicGraphBuilder(States& states) : states(states) {}

		template<typename F>
		LogicNode& Create(F&& f, std::string name)
		{
			auto fetchedStates = FetchFor(states, f);
			LogicNode& node = _nodes[name];
			node._name = name;
			node._dispatcher = [fetchedStates, f = std::forward<F>(f)]()
			{
				//TODO: add support for parallel dispatch
				Dispatch(fetchedStates, f);
			};
			node._reads.clear();
			node._writes.clear();
			MPL::for_tuple(fetchedStates, [&node](auto &wrapper)
			{
				using type = decltype(wrapper);

				if constexpr(MPL::is_const_v<type>)
					node._reads.push_back(typeid(type).hash_code());
				else
					node._writes.push_back(typeid(type).hash_code());
			});
			return node;
		}

		void Build(LogicGraph& graph)
		{
			BuildSuccessor();
			std::size_t size = _flattenNodes.size();
			graph._logicNodes.reserve(size);
			for (int i = size - 1; i >= 0; i--)
			{
				auto& node = _flattenNodes[i];
				graph._logicNodes.emplace_back(graph._logicGraph, [f = std::move(node->_dispatcher)](tbb::flow::continue_msg) { f(); });
				node->_graphNode = &graph._logicNodes[size-i-1];
				for (auto &succ : node->_successors)
					tbb::flow::make_edge(*node->_graphNode, *succ->_graphNode);
			}
			for (auto &node : _entry)
			{
				tbb::flow::make_edge(graph._startNode, *node->_graphNode);
			}
		}
	};
}