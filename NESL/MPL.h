#pragma once
#include <type_traits>
#include <utility>

namespace MPL {
	template <template <typename...> class TNewName, typename T> struct rewrap;

	// "Renames" `TOldName<Ts...>` to `TNewName<Ts...>`.
	template <template <typename...> class TNewName,
		template <typename...> class TOldName, typename... Ts>
	struct rewrap<TNewName, TOldName<Ts...>> {
		using type = TNewName<Ts...>;
	};

	template <template <typename...> class TNewName, typename T>
	using rewrap_t = typename rewrap<TNewName, T>::type;

	template <typename... Ts> struct typelist {};

	template <typename T> struct type_t { using type = T; };

	template <typename...> struct concat;

	// Interface type alias.
	template <typename... TTypeLists>
	using concat_t = typename concat<TTypeLists...>::type;

	// Concat base case: single non-empty list.
	template <typename... Ts> struct concat<typelist<Ts...>> {
		using type = typelist<Ts...>;
	};

	// Concat recursive case: multiple lists.
	template <typename... Ts0, typename... Ts1, typename... TRest>
	struct concat<typelist<Ts0...>, typelist<Ts1...>, TRest...> {
		using type = concat_t<typelist<Ts0..., Ts1...>, TRest...>;
	};

	template <template <typename> class, typename> struct map;

	template <template <typename> class F, typename... Ts>
	struct map<F, typelist<Ts...>> {
		using type = typelist<F<Ts>...>;
	};

	template <template <typename> class F, typename T>
	using map_t = typename map<F, T>::type;

	template <template <typename> class, typename> struct fliter;

	template <template <typename> class F> struct fliter<F, typelist<>> {
		using type = typelist<>;
	};

	template <template <typename> class F, typename T, typename... Ts>
	struct fliter<F, typelist<T, Ts...>> {
		using type = std::conditional_t<
			F<T>::value,
			concat_t<typelist<T>, typename fliter<F, typelist<Ts...>>::type>,
			typename fliter<F, typelist<Ts...>>::type>;
	};

	template <template <typename> class F, typename T>
	using fliter_t = typename fliter<F, T>::type;

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

	template <typename... Ts> constexpr bool or_(Ts... vs) { return (... || vs); }

	template <typename T, typename Typelist> struct contain;

	template <typename T, typename... Ts> struct contain<T, typelist<Ts...>> {
		static constexpr auto value = or_(std::is_same_v<T, Ts>...);
	};

	template <typename T, typename U>
	constexpr bool contain_v = contain<T, U>::value;

	template <typename> struct unique;

	template <> struct unique<typelist<>> { using type = typelist<>; };

	template <typename T, typename... Ts> struct unique<typelist<T, Ts...>> {
		using type = std::conditional_t<
			contain_v<T, typelist<Ts...>>, typename unique<typelist<Ts...>>::type,
			concat_t<typelist<T>, typename unique<typelist<Ts...>>::type>>;
	};

	template <typename T> using unique_t = typename unique<T>::type;

	template <typename T, typename U> struct intersection {
		template <typename Y> using is_in_U = contain<Y, U>;
		using type = fliter_t<is_in_U, T>;
	};

	template <typename T, typename U>
	using intersection_t = typename intersection<T, U>::type;

	template <typename T, typename U> using union_t = unique_t<concat_t<T, U>>;

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

	template <typename F> struct generic_function_trait {
		using impl = generic_function_trait<decltype(&F::operator())>;
		using return_type = typename impl::return_type;
		using argument_type = typename impl::argument_type;
	};

	template <typename F, typename Ret, typename... Args>
	struct generic_function_trait<Ret(F::*)(Args...)> {
		using return_type = Ret;
		using argument_type = typelist<Args...>;
	};

	template <typename F, typename Ret, typename... Args>
	struct generic_function_trait<Ret(F::*)(Args...) const> {
		using return_type = Ret;
		using argument_type = typelist<Args...>;
	};

	template <typename Ret, typename... Args>
	struct generic_function_trait<Ret(*)(Args...)> {
		using return_type = Ret;
		using argument_type = typelist<Args...>;
	};

	template <typename T, typename U = void>
	struct is_complete : std::false_type {};

	template <typename T>
	struct is_complete<T, std::void_t<decltype(sizeof(T))>> : std::true_type {};

	template <typename F, typename Tuple, std::size_t... indices>
	constexpr decltype(auto) for_tuple_impl(Tuple &&tuple, F &&f,
		std::index_sequence<indices...>) {
		return (void)std::initializer_list<int>{
			(f(std::get<indices>(std::forward<Tuple>(tuple))), 0)...};
	}

	template <typename F, typename Tuple>
	constexpr decltype(auto) for_tuple(Tuple &&tuple, F &&f) {
		using Indices =
			std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>;
		return for_tuple_impl(std::forward<Tuple>(tuple), std::forward<F>(f),
			Indices{});
	}

	template <typename T, typename F> constexpr decltype(auto) for_types(F &&f) {
		if constexpr (size<T>{} > 0u)
			return for_tuple(rewrap_t<std::tuple, map_t<type_t, T>>{}, f);
	}
}