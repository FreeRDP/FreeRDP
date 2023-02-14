#include <filesystem>
#include <fstream>

#include "font_writer.hpp"
#include "opensans_variable_font.hpp"

std::string create_and_return_temorary_font()
{
	static std::string name;

	if (name.empty())
	{
		auto path = std::filesystem::temp_directory_path();
		path /= "font.ttf";

		std::ofstream ofs(path, std::ios::binary);
		ofs.write(reinterpret_cast<const char*>(font_buffer.data()), font_buffer.size());

		name = path.u8string();
	}
	return name;
}
