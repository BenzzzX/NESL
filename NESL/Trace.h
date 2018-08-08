#pragma once
#include <type_traits>
#include "HBV.h"

namespace ESL
{
	enum Trace : uint8_t
	{
		Create = 0b00001,
		Remove = 0b00010,
		Borrow = 0b00100,
		CR	   = 0b00011,
		CB     = 0b00101,
		RB     = 0b00110,
		CRB    = 0b00111,
		HasNot = 0b01000,
		Has    = 0b10000
	};

	//example: Tag<ELocation, Create>
	template<typename T, Trace type>
	struct Filter_t 
	{
		static constexpr Trace type = type;
		using target = T;
	};

#define FHas(T) ESL::Filter_t<T, ESL::Has> = {}
#define FCreated(T) ESL::Filter_t<T, ESL::Create> = {}
#define FRemoved(T) ESL::Filter_t<T, ESL::Remove> = {}
#define FBorrowed(T) ESL::Filter_t<T, Borrow> = {}
#define FHasNot(T) ESL::Filter_t<T, ESL::HasNot> = {}

#define Filter(T, W) ESL::Filter_t<T, W> = {}

	template<typename T>
	struct is_filter : std::false_type {};

	template<typename T, Trace type>
	struct is_filter<Filter_t<T, type>> : std::true_type {};

	template<Trace type>
	struct Tracer
	{
		static_assert(type < 0b1001 && type > 0, "wrong type!");
		HBV::bit_vector flag{ 10u };
		using bit_vector_and2 = decltype(HBV::compose(HBV::and_op, HBV::bit_vector{}, HBV::bit_vector{}));

		void Create(HBV::index_t e)
		{
			if (flag.size() <= e)
			{
				if constexpr(type & Trace::HasNot)
					flag.grow_to(e + 64 * 64, true);
				else
					flag.grow_to(e + 64 * 64);

			}
				
			if constexpr(type & Trace::Create)
				flag.set(e, true);
			if constexpr(type & Trace::HasNot)
				flag.set(e, false);
		}

		void Change(HBV::index_t e)
		{
			if constexpr(type & Trace::Borrow)
				flag.set(e, true);

		}

		void Remove(HBV::index_t e)
		{
			if constexpr(type & Trace::Remove)
				flag.set(e, true);
			if constexpr(type & Trace::HasNot)
				flag.set(e, true);
		}

		void BatchCreate(index_t begin, index_t end)
		{
			if (flag.size() <= end)
				flag.grow_to(end);
			if constexpr(type & Trace::Create)
				flag.set_range(begin, end, true);
			if constexpr(type & Trace::HasNot)
				flag.set_range(begin, end, false);
		}

		void BatchRemove(const bit_vector_and2& remove)
		{
			if constexpr(type & Trace::Remove)
				flag.merge(remove);
			if constexpr(type & Trace::HasNot)
				flag.merge(remove);
		}

		void Reset()
		{
			if constexpr(!(type & Trace::HasNot))
				flag.clear();
		}
	};

	template<size_t bits, size_t... is>
	constexpr bool FitTracer()
	{
		return bits & ((1 << is) | ...);
	}

	template<size_t... is, typename Tuple>
	__forceinline decltype(auto) ComposeTracerFlags(const Tuple& tuple)
	{
		return HBV::compose(HBV::or_op, (std::get<Tracer<(Trace)is>>(tuple).flag)...);
	}

	template<Trace type, Trace... types, typename Tuple>
	__forceinline decltype(auto) ComposeTracer(const Tuple& tuple)
	{
		static_assert(sizeof...(types) > 0, "no tracer!");
		constexpr auto covered = (types | ... | 0);
		static_assert(type & covered, "no available tracer!");
		constexpr size_t bits = ((1 << types) | ...);
		if constexpr(type & Trace::HasNot)
		{
			constexpr auto rest = type & 0b111;
			if constexpr(rest == Trace::RB)
			{
				if constexpr(FitTracer<bits, 8, 6>()) return ComposeTracerFlags<8, 6>(tuple);
				else return ComposeTracerFlags<8, 2, 3>(tuple);
			}
			else if constexpr(rest == Trace::Borrow)
				return ComposeTracerFlags<8, 3>(tuple);
			else if constexpr(rest == Trace::Remove)
				return ComposeTracerFlags<8, 2>(tuple);
		}
		else
		{
			if constexpr(type == Trace::CRB)
			{
#define TRYFIT(...)	constexpr(FitTracer<bits, __VA_ARGS__>()) return ComposeTracerFlags<__VA_ARGS__>(tuple)
				if TRYFIT(7);
				else if TRYFIT(1, 6);
				else if TRYFIT(2, 5);
				else if TRYFIT(3, 4);
				else if TRYFIT(1, 2, 3);
				else if TRYFIT(5, 6);
				else if TRYFIT(4, 6);
				else return ComposeTracerFlags<4, 5>(tuple);
			}
			else if constexpr(type == Trace::RB)
			{
				if TRYFIT(6);
				else return ComposeTracerFlags<2, 3>(tuple);
			}
			else if constexpr(type == Trace::CB)
			{
				if TRYFIT(5);
				else return ComposeTracerFlags<1, 3>(tuple);
			}
			else if constexpr(type == Trace::CR)
			{
				if TRYFIT(4);
				else return ComposeTracerFlags<1, 2>(tuple);
			}
			else if constexpr(type == Trace::Borrow)
				return ComposeTracerFlags<3>(tuple);
			else if constexpr(type == Trace::Remove)
				return ComposeTracerFlags<2>(tuple);
			else if constexpr(type == Trace::Create)
				return ComposeTracerFlags<1>(tuple);
#undef TRYFIT
		}
	}

	
}