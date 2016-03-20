// compile with C++ 14 (for auto return type deduction) + OpenMP
// e.g. clang++ -g3 -std=c++14 -fopenmp space.cpp

#include <functional>
#include <iostream>
#include <tuple>

#include <cassert>

#include <omp.h>

// prototype for N-DIM iteration
template <int DIM, typename space> struct iteration;

// simple 2-D iteration
template <typename spaceT> struct iteration<2, spaceT> {
	int i, j;

	std::function<void(int &, int &, spaceT)> _order;

	spaceT _space;

	iteration<2, spaceT>() = delete;
	iteration<2, spaceT>(const iteration<2, spaceT> &) = default;

	iteration<2, spaceT>(spaceT s) : i(s.start_i), j(s.start_j), _space(s) {}

	bool operator!=(const iteration<2, spaceT> &rhs) const noexcept {
		return rhs.i != i || rhs.j != j || rhs._space != _space;
	}

	void operator++() noexcept {
		assert(_order);
		_order(i, j, _space);
	}

	auto operator*() const noexcept { return std::pair<int, int>(i, j); }
};

// space1d * space1d = space2d?
struct space2d {
	int start_i, end_i;
	int start_j, end_j;
	space2d(const int start_i_, const int end_i_, const int start_j_, const int end_j_)
		: start_i(start_i_), end_i(end_i_), start_j(start_j_), end_j(end_j_) {}

	bool operator!=(space2d rhs) const noexcept {
		return rhs.start_i != start_i || rhs.start_j != start_j || rhs.end_i != end_i || rhs.end_j != end_j;
	}
};

template <typename spaceT> struct _rm_order {
	spaceT _space;

	_rm_order(spaceT s) : _space(s) {}

	// TODO generalize it, only ok if spaceT === space2d
	static void next(int &i, int &j, spaceT space) {
		++i;
		if (i >= space.end_i) {
			i = space.start_i;
			++j;
			if (j >= space.end_j) {
				i = space.end_i;
				j = space.end_j;
			}
		}
	}

	auto begin() const noexcept {
		// FIXME infere dimension from spaceT
		auto temp = iteration<2, spaceT>(_space);
		temp._order = next;

		return temp;
	}

	auto end() const noexcept {
		// FIXME infere dimension from spaceT
		auto temp = iteration<2, spaceT>(_space);
		temp.i = _space.end_i;
		temp.j = _space.end_j;
		temp._order = next;

		return temp;
	}
};

// just a little helper
template <typename T> _rm_order<T> rm_order(T instance) { return _rm_order<T>(instance); }

// TODO should allow to select the dimonsion used to partition
template <typename orderT> struct _static_partition {
	orderT _order;
	_static_partition() = delete;

	_static_partition(orderT o) : _order(o) {
		int id = omp_get_thread_num();
		int threads = omp_get_num_threads();
		int size = _order._space.end_i - _order._space.start_i;
		_order._space.start_i = (size / threads) * id;
		if (id != threads - 1) _order._space.end_i = size / threads * (id + 1);
	}

	auto begin() const noexcept { return _order.begin(); }

	auto end() const noexcept { return _order.end(); }
};

// just a little helper
template <typename T> _static_partition<T> static_partition(T instance) { return _static_partition<T>(instance); }

int main(int argc, char const *argv[]) {

	double arr1[100][100], arr2[100][100];

#pragma omp parallel
	for (auto i : static_partition(rm_order(space2d(1, 99, 1, 99)))) {
		arr1[i.first][i.second] = (arr2[i.first - 1][i.second] + arr2[i.first + 1][i.second] +
								   arr2[i.first][i.second - 1] + arr2[i.first][i.second + 1]) /
								  4;
	}
	return 0;
}