#include <array>
#include <vector>
#include <sys/types.h>




using image_shape = std::array<ssize_t, 2>;
using dynamic_shape = std::vector<ssize_t>;

enum class DetectorType { Jungfrau, Eiger, Mythen3, Moench };

enum class TimingMode {Auto, Trigger};
DetectorType StringTo(std::string_view name);