#include <random>

namespace utils
{
	template <typename IntegerType = int>
	class UniformIntRandom
	{
		std::mt19937 gen{ std::random_device{}() };
	public:

		explicit UniformIntRandom(std::mt19937::result_type seed) : gen(seed) {}

		UniformIntRandom() = default;
		UniformIntRandom(const UniformIntRandom&) = default;
		UniformIntRandom(UniformIntRandom&&) = default;
		UniformIntRandom& operator=(const UniformIntRandom&) = default;
		UniformIntRandom& operator=(UniformIntRandom&&) = default;
		~UniformIntRandom() = default;

		IntegerType rand(IntegerType min, IntegerType max)
		{
			return std::uniform_int_distribution<IntegerType>{min, max}(gen);
		}
	};

	template <typename IntegerType = int>
	IntegerType uniform_int_rand(IntegerType min, IntegerType max)
	{
		static UniformIntRandom<IntegerType> uir{};
		return uir.rand(min, max);
	}

}