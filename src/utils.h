#include <random>

namespace utils
{
	template <typename IntegerType = int>
	IntegerType uniform_int_rand(IntegerType min, IntegerType max)
	{
		static std::mt19937 gen{ std::random_device{}() };
		return std::uniform_int_distribution<IntegerType>{min, max}(gen);
	}
}