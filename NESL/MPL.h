#pragma once
#include <type_traits>
#include <utility>

namespace MPL 
{
	template <template <typename...> class TNewName, typename T> struct rewrap;

	// "Renames" `TOldName<Ts...>` to `TNewName<Ts...>`.
	template 
	<
		template <typename...> class TNewName,
		template <typename...> class TOldName, 
		typename... Ts
	>
	struct rewrap<TNewName, TOldName<Ts...>> 
	{
		using type = TNewName<Ts...>;
	};

	template
	<
		template <typename...> class TNewName, 
		typename T
	>
	using rewrap_t = typename rewrap<TNewName, T>::type;

	

	template <typename... Ts> struct typelist {};

	template <typename T> 
	struct type_t { using type = T; };

	template <typename...> struct concat;

	// Interface type alias.
	template <typename... TTypeLists>
	using concat_t = typename concat<TTypeLists...>::type;

	// Concat base case: single non-empty list.
	template <typename... Ts> 
	struct concat<typelist<Ts...>> 
	{
		using type = typelist<Ts...>;
	};

	// Concat recursive case: multiple lists.
	template 
	<
		typename... Ts0, 
		typename... Ts1, 
		typename... TRest
	>
	struct concat<typelist<Ts0...>, typelist<Ts1...>, TRest...> 
	{
		using type = concat_t<typelist<Ts0..., Ts1...>, TRest...>;
	};

	template <template <typename> class, typename> struct map;

	template <template <typename> class F, typename... Ts>
	struct map<F, typelist<Ts...>> 
	{
		using type = typelist<F<Ts>...>;
	};



	template <template <typename> class F, typename T>
	using map_t = typename map<F, T>::type;

	template <template <typename> class, typename> struct filter;

	template <template <typename> class F, typename T>
	using filter_t = typename filter<F, T>::type;

	template <template <typename> class F> 
	struct filter<F, typelist<>> 
	{ using type = typelist<>; };

	template <template <typename> class F, typename T, typename... Ts>
	struct filter<F, typelist<T, Ts...>> 
	{
		using type = std::conditional_t
		<
			F<T>::value,
			concat_t<typelist<T>, filter_t<F, typelist<Ts...>>>,
			filter_t<F, typelist<Ts...>>
		>;
	};

	

	template <typename... Ts>
	struct size_raw : std::integral_constant<std::size_t, sizeof...(Ts)> {};

	template <typename T> using size = rewrap_t<size_raw, T>;

	template <typename, typename> struct index;

	template <typename T, typename... Ts>
	struct index<T, typelist<T, Ts...>> : std::integral_constant<std::size_t, 0> {};

	template <typename T, typename TOther, typename... Ts>
	struct index<T, typelist<TOther, Ts...>>
		: std::integral_constant<std::size_t,
		1 + index<T, typelist<Ts...>>::value> {};


	template <typename T, typename Typelist> 
	struct contain : std::false_type {};

	template <typename T, typename... Ts> 
	struct contain<T, typelist<Ts...>> 
	{
		static constexpr auto value = std::disjunction_v<std::is_same<T, Ts>...>;
	};

	template <typename T, typename U>
	constexpr bool contain_v = contain<T, U>::value;

	template <typename> struct unique;

	template <> 
	struct unique<typelist<>> 
	{ using type = typelist<>; };

	template <typename T, typename... Ts> 
	struct unique<typelist<T, Ts...>> 
	{
		using type = std::conditional_t
		<
			contain_v<T, typelist<Ts...>>, 
			typename unique<typelist<Ts...>>::type,
			concat_t<typelist<T>, typename unique<typelist<Ts...>>::type>
		>;
	};

	template <typename T> 
	using unique_t = typename unique<T>::type;

	template <typename T, typename U> 
	struct intersection 
	{
		template <typename Y> using is_in_U = contain<Y, U>;
		using type = filter_t<is_in_U, T>;
	};

	template <typename T, typename U>
	using intersection_t = typename intersection<T, U>::type;

	template <typename T, typename U> 
	using union_t = unique_t<concat_t<T, U>>;

	template <typename T, typename U> 
	struct remove 
	{
		template <typename Y> using is_not_T = std::negation<std::is_same<Y, T>>;
		using type = filter_t<is_not_T, U>;
	};

	template <typename T, typename U>
	using remove_t = typename remove<T, U>::type;

#if 0
	template<class Curr, class Func>
	struct folder
	{
		Func _fun;
		constexpr folder(Func fun) : _fun(fun) {}

		constexpr Curr result() { return {}; }

		template<class T>
		constexpr decltype(auto) operator>>(T v) {
			return folder<decltype(_fun(Curr{}, v)), Func > {_fun};
		}
	};

	template<class T, class Func>
	constexpr decltype(auto) make_folder(Func f, T init = {}) {
		return folder<T, Func>{f};
	}
#endif

	template <typename F> 
	struct generic_function_trait 
	{
		using impl = generic_function_trait<decltype(&F::operator())>;
		using return_type = typename impl::return_type;
		using argument_type = typename impl::argument_type;
	};

	template <typename F, typename Ret, typename... Args>
	struct generic_function_trait<Ret(F::*)(Args...)> 
	{
		using return_type = Ret;
		using argument_type = typelist<Args...>;
	};

	template <typename F, typename Ret, typename... Args>
	struct generic_function_trait<Ret(F::*)(Args...) const> 
	{
		using return_type = Ret;
		using argument_type = typelist<Args...>;
	};

	template <typename Ret, typename... Args>
	struct generic_function_trait<Ret(*)(Args...)> 
	{
		using return_type = Ret;
		using argument_type = typelist<Args...>;
	};

	template <typename T, typename U = void>
	struct is_complete : std::false_type {};

	template <typename T>
	struct is_complete<T, std::void_t<decltype(sizeof(T))>> : std::true_type {};

	template <typename F, typename Tuple, std::size_t... indices>
	constexpr decltype(auto) for_tuple_impl(Tuple &&tuple, F &&f, std::index_sequence<indices...>) 
	{
		return (void)std::initializer_list<int>{
			(f(std::get<indices>(std::forward<Tuple>(tuple))), 0)...};
	}

	template <typename F, typename Tuple>
	constexpr decltype(auto) for_tuple(Tuple &&tuple, F &&f) 
	{
		using Indices =
			std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>;
		return for_tuple_impl(std::forward<Tuple>(tuple), std::forward<F>(f),
			Indices{});
	}

	template <typename T, typename F> 
	constexpr decltype(auto) for_types(F &&f) 
	{
		if constexpr (size<T>{} > 0u)
			return for_tuple(rewrap_t<std::tuple, map_t<type_t, T>>{}, f);
	}

	template<typename T>
	struct remove_const 
	{ using type = std::remove_const_t<T>; };

	template<typename T>
	struct remove_const<T const&> 
	{ using type = T & ; };

	template<typename T>
	struct remove_const<T const&&> 
	{ using type = T && ; };

	template<typename T>
	using remove_const_t = typename remove_const<T>::type;

	template<typename T>
	struct add_const 
	{ using type = std::add_const_t<T>; };

	template<typename T>
	struct add_const<T &> 
	{ using type = T const& ; };

	template<typename T>
	struct add_const<T &&> 
	{ using type = T const&& ; };

	template<typename T>
	using add_const_t = typename add_const<T>::type;

	template<typename T>
	struct is_const : std::is_const<T> { };

	template<typename T>
	struct is_const<T const&> : std::true_type { };

	template<typename T>
	struct is_const<T const&&> : std::true_type { };

	template<typename T>
	constexpr bool is_const_v = is_const<T>::value;

	template<typename T, typename C>
	static constexpr decltype(auto) nonstrict_get(C& c)
	{
		using nonstrict_type = std::conditional_t
		<
			MPL::contain_v
			<
				remove_const_t<T>,
				MPL::rewrap_t<MPL::typelist, C>
			>,
			remove_const_t<T>,
			T
		>;
		return std::get<nonstrict_type>(c);
	}

	template<typename T>
	const char* type_name()
	{
		return typeid(type_t<T>).name();
	}

	template <template <class...> class Trait, class Enabler, class... Args>
	struct is_detected_helper : std::false_type {};

	template <template <class...> class Trait, class... Args>
	struct is_detected_helper<Trait, std::void_t<Trait<Args...>>, Args...> : std::true_type {};

	template <template <class...> class Trait, class... Args>
	using is_detected = typename is_detected_helper<Trait, void, Args...>::type;
}