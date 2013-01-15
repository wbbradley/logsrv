#pragma once
#include <string>
#include <tr1/memory>

using std::tr1::shared_ptr;

#define debug_ex(x)

#ifdef DEBUG
#define debug(x) x
#else
#define debug(x)
#endif

bool check_errno();
double get_current_time();

inline bool mask(int grf, int grf_mask)
{
	return grf & grf_mask;
}

template <typename T>
inline size_t countof(const T &t)
{
	return t.size();
}

template <typename T, size_t N>
constexpr size_t countof(T (&array)[N])
{
	return N;
}

