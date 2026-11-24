#pragma once

namespace Ini
{
	class File
	{
	public:
		explicit File(std::filesystem::path a_path);

		[[nodiscard]] bool Found() const { return m_found; }

		[[nodiscard]] bool  Bool(const char* a_section, const char* a_key, bool a_default) const;
		[[nodiscard]] float Float(const char* a_section, const char* a_key, float a_default) const;
		[[nodiscard]] int   Int(const char* a_section, const char* a_key, int a_default) const;

		void Set(const char* a_section, const char* a_key, bool a_value);
		bool Write() const;

	private:
		[[nodiscard]] const std::string* Find(const char* a_section, const char* a_key) const;

		struct Line
		{
			std::string text;
			std::string section;
			std::string key;
			std::string value;
		};

		std::filesystem::path m_path;
		std::vector<Line>     m_lines;
		bool                  m_found = false;
	};
}
