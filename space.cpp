// compile with C++ 14 (for auto return type deduction) + OpenMP
// e.g. clang++ -g3 -std=c++14 -fopenmp space.cpp

#include <functional>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <array>

#include <cassert>

#include <omp.h>

namespace impl {
template <int N, typename T> struct array_to_tuple {
	constexpr static auto get(const T &arr) noexcept {
		return std::tuple_cat(std::make_tuple(arr[std::tuple_size<T>::value - N]), array_to_tuple<N - 1, T>::get(arr));
	}
};

template <typename T> struct array_to_tuple<1, T> {
	constexpr static auto get(const T &arr) noexcept { return std::make_tuple(arr[std::tuple_size<T>::value - 1]); }
};
}

template <int DIM, typename spaceT> struct iteration {
	static constexpr int dim = DIM;

	std::array<int, DIM> index;
	std::function<void(decltype(index) &, spaceT)> order;

	spaceT _space;

	iteration<DIM, spaceT>() = delete;

	iteration<DIM, spaceT>(const iteration<DIM, spaceT> &) = default;
	iteration<DIM, spaceT>(spaceT &&s) : index(s.start), _space(std::forward<spaceT>(s)) {}

	iteration<DIM, spaceT>(const spaceT &s) : index(s.start), _space(s) {}

	bool operator!=(const iteration<DIM, spaceT> &rhs) const noexcept {
		return rhs.index != index || rhs._space != _space;
	}

	void operator++() noexcept {
		assert(order);
		order(index, _space);
	}

	auto operator*() const noexcept { return impl::array_to_tuple<DIM, decltype(index)>::get(index); }
};

// prevent anyone from using a space with 0 dimension
template <typename spaceT> struct iteration<0, spaceT>;

namespace impl {
// dense_space<1> * dense_space<1> = dense_space<2>?
template <int DIM> struct dense_space {
	static constexpr int dim = DIM;

	std::array<int, DIM> start, limit;

	dense_space(const dense_space<DIM> &s) = default;
	dense_space(dense_space<DIM> &&s) = default;

	// first parameter const int to make sure it is not used as a copy constructor
	template <typename... argsT> dense_space(const int i, argsT &&... args) {
		static_assert(sizeof...(args) + 1 == DIM * 2, "Missing constructor parameters for dense_space.");
		init<DIM>(i, std::forward<argsT>(args)...);
	}

	bool operator!=(const dense_space<DIM> &rhs) const noexcept { return rhs.start != start || rhs.limit != limit; }

	auto begin() const noexcept {
		auto temp = iteration<DIM, dense_space<DIM>>(*this);
		temp.index = start;
		return temp;
	}
	auto end() const noexcept {
		auto temp = iteration<DIM, dense_space<DIM>>(*this);
		temp.index = limit;
		return temp;
	}

  private:
	template <int N, typename... argsT> void init(const int _start, const int _end, argsT &&... args) noexcept {
		static_assert(sizeof...(args) == (N - 1) * 2, "Internal error. Something is broken with our constructor.");
		start[DIM - N] = _start;
		limit[DIM - N] = _end;
		init<N - 1>(std::forward<argsT>(args)...);
	}

	template <int N> void init(const int _start, const int _end) {
		start[DIM - N] = _start;
		limit[DIM - N] = _end;
	}
};
}

// just a little helper
template <typename... argsT> auto dense_space(argsT &&... args) {
	static_assert(sizeof...(args) % 2 == 0, "Wrong number of parameters for dense_space");
	return impl::dense_space<sizeof...(argsT) / 2>(std::forward<argsT>(args)...);
}

namespace impl {
template <int N, typename T, typename spaceT> struct cm_next {
	static void get(decltype(spaceT::start) &arr, const spaceT &space) noexcept {
		constexpr int index = spaceT::dim - N;
		++arr[index];
		if (arr[index] >= space.limit[index]) {
			arr[index] = space.start[index];
			impl::cm_next<N - 1, T, spaceT>::get(arr, space);
		}
	}
};

template <typename T, typename spaceT> struct cm_next<1, T, spaceT> {
	static void get(decltype(spaceT::start) &arr, const spaceT &space) noexcept {
		constexpr int index = spaceT::dim - 1;
		++arr[index];
		if (arr[index] >= space.limit[index]) {
			arr = space.limit;
		}
	}
};

template <typename spaceT> struct cm_order {
	spaceT _space; // could be a partitioned space

	cm_order() = delete;
	cm_order(const cm_order<spaceT> &) = default;
	cm_order(cm_order<spaceT> &&) = default;

	cm_order(const spaceT &s) : _space(s) {}
	cm_order(spaceT &&s) : _space(std::forward<spaceT>(s)) {}

	auto begin() const noexcept {
		iteration<spaceT::dim, spaceT> temp(_space);
		temp.order = impl::cm_next<spaceT::dim, decltype(_space.start), spaceT>::get;

		return temp;
	}

	auto end() const noexcept {
		iteration<spaceT::dim, spaceT> temp(_space);
		temp.index = _space.limit;
		temp.order = impl::cm_next<spaceT::dim, decltype(_space.start), spaceT>::get;

		return temp;
	}
};
}

// just a little helper
template <typename T> auto cm_order(T &&instance) { return impl::cm_order<T>(std::forward<T>(instance)); }

namespace impl {
template <int N, typename T, typename spaceT> struct rm_next {
	static void get(decltype(spaceT::start) &arr, const spaceT &space) noexcept {
		constexpr int index = N - 1;
		++arr[index];
		if (arr[index] >= space.limit[index]) {
			arr[index] = space.start[index];
			impl::rm_next<N - 1, T, spaceT>::get(arr, space);
		}
	}
};

template <typename T, typename spaceT> struct rm_next<1, T, spaceT> {
	static void get(decltype(spaceT::start) &arr, const spaceT &space) noexcept {
		constexpr int index = 1 - 1;
		++arr[index];
		if (arr[index] >= space.limit[index]) {
			arr = space.limit;
		}
	}
};

template <typename spaceT> struct rm_order {
	spaceT _space; // could be a partitioned space

	rm_order() = delete;
	rm_order(const rm_order<spaceT> &) = default;
	rm_order(rm_order<spaceT> &&) = default;

	rm_order(const spaceT &s) : _space(s) {}
	rm_order(spaceT &&s) : _space(std::forward<spaceT>(s)) {}

	auto begin() const noexcept {
		iteration<spaceT::dim, spaceT> temp(_space);
		temp.order = impl::rm_next<spaceT::dim, decltype(_space.start), spaceT>::get;

		return temp;
	}

	auto end() const noexcept {
		iteration<spaceT::dim, spaceT> temp(_space);
		temp.index = _space.limit;
		temp.order = impl::rm_next<spaceT::dim, decltype(_space.start), spaceT>::get;

		return temp;
	}
};
}

// just a little helper
template <typename T> auto rm_order(T &&instance) { return impl::rm_order<T>(std::forward<T>(instance)); }

namespace impl {
template <typename spaceT> struct static_partition : public spaceT {
	static_partition() = delete;
	static_partition(const static_partition<spaceT> &) = default;
	static_partition(static_partition<spaceT> &&) = default;

	static_partition(const int dim, const spaceT &o) : spaceT(o) { partition(dim); }
	static_partition(const int dim, spaceT &&o) : spaceT(std::forward<spaceT>(o)) { partition(dim); }

  private:
	void partition(const int dim) noexcept {
		int id = omp_get_thread_num();
		int threads = omp_get_num_threads();
		int size = spaceT::limit[dim] - spaceT::start[dim];
		spaceT::start[dim] = (size / threads) * id;
		if (id != threads - 1) spaceT::limit[dim] = size / threads * (id + 1);
	}
};
}

// just a little helper
template <typename T> auto static_partition(const int dim, T &&instance) {
	return impl::static_partition<T>(dim, std::forward<T>(instance));
}

int main(int argc, char const *argv[]) {

	double arr1[100][100], arr2[100][100];

#pragma omp parallel
	{
		int i, j;
		for (const auto &iteration : cm_order(static_partition(0, dense_space(1, 99, 1, 99)))) {
			std::tie(i, j) = std::move(iteration);
			//#pragma omp critical
			// std::cout << omp_get_thread_num() << " - " << i << " - " << j << std::endl;
			arr1[i][j] = (arr2[i - 1][j] + arr2[i + 1][j] + arr2[i][j - 1] + arr2[i][j + 1]) / 4;
		}
	}
	return 0;
}
