#pragma once
#include "States.h"
#include "MPL.h"
#include "Dispather.h"
#include <unordered_set>
#include <queue>
#include "small_vector.h"
#include <fstream>
#include <iostream>
#include <tbb\tbb.h>
#include "Parallel.h"
#include "Flatten.h"


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

	//TODO: Support replace scheduled logic
	class LogicGraphBuilder
	{
		struct StateNode;
		class LogicNode
		{
			std::function<void(void)> _dispatcher;
			chobo::small_vector<std::size_t, 8> _reads;
			chobo::small_vector<std::size_t, 8> _writes;

			chobo::small_vector<LogicNode*, 8> _successors;
			std::unordered_set<LogicNode*> _manualSuccessors;
			std::unordered_set<LogicNode*> _implicitSuccessors;
			tbb::flow::continue_node<tbb::flow::continue_msg>* _graphNode;
			std::string _name;
			int32_t _refcount = 0;
			bool _is_parallel = false;
			friend LogicGraphBuilder;

		public:
			chobo::small_vector<std::string, 8> dependencies;

			template<typename... Ts>
			void After(Ts&&... args)
			{
				std::initializer_list<int32_t> _{ (dependencies.emplace_back(std::forward<Ts>(args)),0)... };
			}
		};

		struct StateInfo
		{
			const char* _name;
			bool _isGlobal;
		};

		struct StateNode
		{
			size_t _version;
			size_t _id;
			chobo::small_vector<LogicNode*, 8> _readers;
			LogicNode *_owner;
			LogicNode *_writer;
		};

		std::unordered_map<std::size_t, StateInfo> _stateInfos;
		lni::vector<StateNode> _stateNodes;


		std::unordered_map<std::string, LogicNode> _logicNodes;
		lni::vector<LogicNode*> _flattenNodes;
		chobo::small_vector<LogicNode*> _entry;
		States &states;

		void CalculateSuccessor()
		{
			for (auto &i : _logicNodes)
			{
				auto &node = i.second;
				if (node.dependencies.empty())
				{
					_entry.push_back(&node);
					continue;
				}
				for (auto& dn : node.dependencies)
				{
					auto it = _logicNodes.find(dn);
					if (it == _logicNodes.end()) abort();
					it->second._successors.push_back(&node);
					it->second._manualSuccessors.insert(&node);
					node._refcount += 1;
				}
			}
		}

		void AddSuccessor(LogicNode* node, LogicNode* succ, std::size_t id, bool implicit = false)
		{
			auto &succs = node->_successors;
			if (std::find(succs.begin(), succs.end(), succ) == succs.end())
			{
				std::cerr << "Warning: Implicit dependency between [" << succ->_name.c_str() << "] and [" << node->_name.c_str() << "] due to conflict on state ["<< _stateInfos[id]._name <<"].\n";
				succs.push_back(succ); 
				if (implicit) node->_implicitSuccessors.insert(succ);
			}
			else if(!implicit)
			{
				node->_manualSuccessors.erase(succ);
			}
		}


		void ImplicitDependency()
		{
			std::queue<LogicNode*> searching;
			std::unordered_map<std::size_t, std::size_t> states;

			for (auto &node : _entry)
				searching.push(node);

			std::size_t nodeSize = _stateInfos.size();
			for (auto& logicPair : _logicNodes)
			{
				auto& logic = logicPair.second;
				nodeSize += logic._writes.size();
			}
			_stateNodes.reserve(nodeSize);

			for (auto &it : _stateInfos)
			{
				_stateNodes.emplace_back();
				StateNode &state = _stateNodes.back();
				state._version = 0;
				state._id = it.first;
				state._writer = nullptr;
				state._owner = nullptr;
				state._readers.clear();
				states[it.first] = _stateNodes.size() - 1;
			}

			_flattenNodes.reserve(_logicNodes.size() + 1);
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
					if (read == typeid(GlobalState<Entities>).hash_code()) continue;
					std::size_t &id = states[read];
					StateNode &sn = _stateNodes[id];
					sn._readers.push_back(node);
					if(sn._writer)
						AddSuccessor(sn._writer, node, read);
				}

				for (auto &write : node->_writes)
				{
					std::size_t &id = states[write];
					StateNode &sn = _stateNodes[id];
					for (auto &reader : sn._readers)
						AddSuccessor(reader, node, write, true);
					if (sn._writer)
						AddSuccessor(sn._writer, node, write);
					sn._owner = node;

					_stateNodes.emplace_back();
					StateNode &state = _stateNodes.back();
					state._version = sn._version + 1;
					state._id = sn._id;
					state._writer = node;
					state._owner = nullptr;
					state._readers.clear();
					id = _stateNodes.size() - 1;
				}
			}
		}

		void LoopDependency()
		{
			for (auto& node : _logicNodes)
			{
				if (node.second._refcount > 0)
				{
					std::cerr << "Loop dependency with " << node.second._name.c_str() << ".\n";
				}
			}
		}

		template<typename S>
		LogicNode& InitNode(LogicNode& node, const S& fetchedStates)
		{
			node._reads.clear();
			node._writes.clear();
			MPL::for_tuple(fetchedStates, [&node, this](auto &wrapper)
			{
				using type = decltype(wrapper);
				using intern = typename TStateTrait<std::decay_t<type>>::Raw;
				std::size_t id = typeid(type).hash_code();

				if (_stateInfos.find(id) == _stateInfos.end())
				{
					auto& stateInfo = _stateInfos[id];
					stateInfo._name = typeid(intern).name() + 8;
					stateInfo._isGlobal = std::is_same_v<State<intern>, GlobalState<intern>>;
				}

				if constexpr(MPL::is_const_v<type>)
					node._reads.push_back(id);
				else
					node._writes.push_back(id);
			});
			return node;
		}

	public:
		LogicGraphBuilder(States& states) : states(states) {}

		template<typename F>
		LogicNode& Schedule(F&& f, std::string name)
		{
			auto fetchedStates = FetchFor(states, f);
			LogicNode& node = _logicNodes[name];
			node._name = name;
			node._dispatcher = [fetchedStates, f = std::forward<F>(f)]()
			{
				Dispatch(fetchedStates, f);
			};
			return InitNode(node, fetchedStates);
		}

		template<typename F>
		LogicNode& ScheduleParallel(F&& f, std::string name)
		{
			auto fetchedStates = FetchFor(states, f);
			LogicNode& node = _logicNodes[name];
			node._name = name;
			node._is_parallel = true;
			node._dispatcher = [fetchedStates, f = std::forward<F>(f)]()
			{
				DispatchParallel(fetchedStates, f);
			};
			return InitNode(node, fetchedStates);
		}

		template<typename F>
		LogicNode& ScheduleFlatten(F&& f, std::string name)
		{
			auto fetchedStates = FetchFor(states, f);
			LogicNode& node = _logicNodes[name];
			node._name = name;
			node._dispatcher = [fetchedStates, f = std::forward<F>(f)]()
			{
				DispatchFlatten(fetchedStates, f);
			};
			return InitNode(node, fetchedStates);
		}

		void Compile()
		{
			CalculateSuccessor();
			ImplicitDependency();
			LoopDependency();
		}

		void Build(LogicGraph& graph)
		{
			std::size_t size = _flattenNodes.size();
			graph._logicNodes.reserve(size + 1);
			for (long long i = size - 1; i >= 0; i--)
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

		void ExportGraphviz(const std::string& path)
		{
			std::ofstream stream(path);
			stream << "digraph framegraph \n{\n";

			stream << "rankdir = LR\n";
			stream << "bgcolor = black\n\n";
			stream << "node [shape=rectangle, fontname=\"helvetica\", fontsize=12, fontcolor=white]\n\n";

			for (auto& logicPair : _logicNodes)
			{
				auto& logic = logicPair.second;
				stream << "\"" << logic._name << "\" [label=\"" << logic._name << "\", style=bold, color=darkorange]\n";
			}
			stream << "\n";

			for (auto& logicPair : _logicNodes)
			{
				auto& logic = logicPair.second;
				for (auto& l : logic._manualSuccessors)
				{
					stream << "\"" << logic._name << "\" -> { ";
					stream << "\"" << l->_name << "\" ";
					stream << "} [color=white]\n";
				}

				for (auto& l : logic._implicitSuccessors)
				{
					stream << "\"" << logic._name << "\" -> { ";
					stream << "\"" << l->_name << "\" ";
					stream << "} [color=yellow3]\n";
				}
			}

			stream << "\n";

			for (auto& state : _stateNodes)
			{
				if (state._readers.empty() && state._owner == nullptr) continue;
				if (state._id == typeid(GlobalState<Entities>).hash_code()) continue;
				const StateInfo &info = _stateInfos[state._id];
				const char* name = info._name;
				stream << "\"" << name << state._version << "\" [label=\"" << name << "\", style=bold, color= " << (info._isGlobal ? "skyblue" : "steelblue") << "]\n";

				stream << "\"" << name << state._version << "\" -> { ";
				for (auto& logic : state._readers)
					stream << "\"" << logic->_name << "\" ";
				stream << "} [color=seagreen]\n";

				if (state._owner)
				{
					stream << "\"" << name << state._version << "\" -> { ";
					stream << "\"" << state._owner->_name << "\" ";
					stream << "} [color=firebrick]\n";
				}

				if (state._writer)
				{
					stream << "\"" << state._writer->_name << "\" -> { ";
					stream << "\"" << name << state._version << "\" ";
					stream << "} [color=firebrick]\n";
				}
			}
			stream << "}";
		}
	};
}