#pragma once
#include "States.h"
#include "Dispather.h"
#include <unordered_set>
#include <queue>
#include "small_vector.h"
#include <iostream>
#include "Parallel.h"
#include "Flatten.h"

namespace ESL
{
	struct DefaultDispatcher
	{
		template<typename F, typename S>
		__forceinline static void Dispatch(S states, F&& logic)
		{
			ESL::Dispatch(states, logic);
		}
	};

	struct FlattenDispatcher
	{
		template<typename F, typename S>
		__forceinline static void Dispatch(S states, F&& logic)
		{
			ESL::DispatchFlatten(states, logic);
		}
	};

	struct ParallelDispatcher
	{
		template<typename F, typename S>
		__forceinline static void Dispatch(S states, F&& logic)
		{
			ESL::DispatchParallel(states, logic);
		}
	};

	class LogicGraph
	{
		States &_states;
		struct LogicNode
		{
			//��̽ڵ�
			chobo::small_vector<LogicNode*, 20> successors;
			chobo::small_vector<std::size_t, 15> reads;
			chobo::small_vector<std::size_t, 15> writes;
			bool enabled;
			bool parallel;
			std::string name;
			std::size_t inRef;
			std::size_t id;
			std::unordered_set<LogicNode*> prefix;
			chobo::small_vector<LogicNode*, 20> from;
			std::function<void(void)> task;
		};
		std::vector<std::unique_ptr<LogicNode>> _graph;
		std::unordered_map<std::string, LogicNode*> _nodeMap;
		chobo::small_vector<LogicNode*> _entry;
		//��������չƽ���ͼ
		lni::vector<LogicNode*> _flattenNodes;
		bool _checked = false;


		template<typename T>
		using is_const_str = std::is_same<T, const char*>;

		void CheckDependency(LogicNode* node, LogicNode* succ, bool warn)
		{
			auto& prefix = succ->prefix;
			//������ͻȴû������,����
			if (std::find(prefix.begin(), prefix.end(), node) == prefix.end())
			{
				if(warn)
					std::cerr << "Warning: Unsolved dependency between [" << node->name << "] and [" << succ->name << "] due to conflict.\n";
				succ->successors.push_back(succ);
				prefix.insert(node);
			}
		}

		void Flatten()
		{
#define for_in(name) for (std::size_t i = 0; i < name.size(); ++i)
			lni::vector<std::size_t> _inRef;
			_inRef.resize(_graph.size(), 0u);
			_flattenNodes.clear();
			_flattenNodes.reserve(_graph.size());
			//������������ά���Ķ���
			std::queue<LogicNode*> checking;
			//���Ϊ0�Ľڵ�
			for_in(_entry)
				checking.push(_entry[i]);
			//��ʼ������������Ҫ�����
			for_in(_graph)
				_inRef[i] = _graph[i]->inRef;

			while (!checking.empty())
			{
				LogicNode* node = checking.front();
				_flattenNodes.push_back(node);
				checking.pop();
				for (auto succ : node->successors)
					if (--_inRef[succ->id] == 0)
						checking.push(succ);
			}

			//��������Ȼ��Ϊ0�Ľڵ�,���ڻ���
			for_in(_inRef)
				if (_inRef[i] != 0)
					std::cerr << "Loop dependency found, include [" << _graph[i]->name << "].\n";
#undef for_in
		}

		//���,������������ģ�����ɵ��߼�����
		void CheckGraph(bool fix = false)
		{
			if (!fix) Flatten();
			//ÿ��state�ĵ�ǰ�Ĺ���(��ȡ)�߼�
			std::unordered_map<std::size_t, chobo::small_vector<LogicNode*, 20>> readers;
			//ÿ��state�ĵ�ǰ��ռ��(д��)�߼�
			std::unordered_map<std::size_t, LogicNode*> writer;
			for(auto node : _flattenNodes)
			{
				for(auto read : node->reads)
				{
					//���빲��,����������һ����ռ
					readers[read].push_back(node);
					if (auto iter = writer.find(read); iter != writer.end())
						CheckDependency(iter->second, node, !fix);
				}

				for(auto write : node->writes)
				{
					//������֮ǰ�Ĺ������һ����ռ,�����ж�ռ
					if (auto iter = readers.find(write); iter != readers.end() && !iter->second.empty())
					{
						auto& reader = iter->second;
						for(auto r : reader)
							CheckDependency(r, node, !fix);
						reader.clear();
					}
					else if (auto iter = writer.find(write); iter != writer.end())
						CheckDependency(iter->second, node, !fix);
					writer[write] = node;
				}
			}
			_checked = true;

		}

		void TryAddNext(std::string name, LogicNode* next)
		{
			auto iter = _nodeMap.find(name);
			assert(iter != _nodeMap.end());
			iter->second->successors.push_back(next);
			for(auto i : iter->second->prefix)
				next->prefix.insert(i);
			next->prefix.insert(iter->second);
			next->from.push_back(iter->second);
		}

		template<typename T>
		void BuildGraph(T& graph);
	public:
		LogicGraph(States& s) : _states(s) {}

		//�����߼�,��ע��������ϵ
		template<typename Dispatcher = DefaultDispatcher, typename F, typename... Ts>
		void Schedule(F&& f, std::string name, Ts... dependencies)
		{
			_checked = false;
			auto fetchedStates = FetchFor(_states, f);
			_graph.emplace_back(std::make_unique<LogicNode>());
			auto& node = _graph.back();
			if constexpr(sizeof...(Ts) == 0)
				_entry.emplace_back(node.get());
			node->inRef = sizeof...(Ts);
			node->id = _graph.size() - 1;
			node->name = name;
			node->enabled = true;
			node->task = [fetchedStates, f = std::forward<F>(f), node = node.get()]()
			{
				if(node->enabled)
					Dispatcher::Dispatch(fetchedStates, f);
			};
			MPL::for_tuple(fetchedStates, [&node](auto &wrapper)
			{

				using type = decltype(wrapper);
				using intern = typename TStateNonstrict<std::decay_t<type>>::Raw;
				std::size_t id = typeid(type).hash_code();

				if constexpr(MPL::is_const_v<type>)
				{
					//Hack!����Entities�Ķ����
					if (!std::is_same_v<type, const GlobalState<Entities>&>)
						node->reads.push_back(id);
				}
				else
					node->writes.push_back(id);
			});

			_nodeMap[std::move(name)] = node.get();
			std::initializer_list<int> _ = { (TryAddNext(std::move(dependencies), node.get()), 0)... };
		}

		//�����ֶ�������ϵ
		template<typename... Ts>
		void AddDependencies(std::string name, Ts... dependencies)
		{
			auto iter = _nodeMap.find(name);
			assert(iter != _nodeMap.end());
			std::initializer_list<int> _ = { (TryAddNext(std::move(dependencies), iter->second), 0)... };
		}

		//ɾ���Ѱ��ŵ��߼�,ע��!�������Ҫ�ֶ�����������صĹ�ϵ
		void Unschedule(std::string name)
		{
			if (!_checked) CheckGraph();
			auto iter = _nodeMap.find(name);
			assert(iter != _nodeMap.end());
			auto& node = iter->second;
			//��ǰ��ڵ����Ƴ��Լ�
			for (auto a : node->successors)
			{
				//���¼���ǰ׺
				a->prefix.clear();
				for (auto p : a->from)
				{
					for (auto i : p->prefix)
						a->prefix.insert(i);
					a->prefix.insert(p);
				}
				auto& from = a->from;
				from.erase(std::remove(from.begin(), from.end(), node), from.end());
			}
			for (auto a : node->from)
			{
				auto& succs = a->successors;
				succs.erase(std::remove(succs.begin(), succs.end(), node), succs.end());
			}
			_flattenNodes.erase(std::remove(_flattenNodes.begin(), _flattenNodes.end(), node), _flattenNodes.end());
			//ɾ���ڵ�
			auto id = node->id;
			_graph[id].swap(_graph[_graph.size() - 1]);
			_graph[id]->id = id;
			_graph.pop_back();
			//���Խ����޲�
			CheckGraph(true);
		}

		//��ʱ�ر��߼�,ע��!����ɾ��,�߼���Ȼ����
		void Disable(std::string name)
		{ 
			auto iter = _nodeMap.find(name);
			assert(iter != _nodeMap.end());
			iter->second->enabled = false;
		}

		//���¿����߼�
		void Enable(std::string name)
		{
			auto iter = _nodeMap.find(name);
			assert(iter != _nodeMap.end());
			iter->second->enabled = true;
		}

		void Check()
		{
			if (!_checked) CheckGraph();
		}

		template<typename T>
		void Build(T& graph)
		{
			if (!_checked) CheckGraph();
			BuildGraph(graph);
		}
	};
}