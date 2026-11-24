#include "Ini.h"

namespace
{
	std::string Trim(const std::string& a_text)
	{
		const auto first = a_text.find_first_not_of(" \t\r\n");
		if (first == std::string::npos) {
			return {};
		}
		return a_text.substr(first, a_text.find_last_not_of(" \t\r\n") - first + 1);
	}

	const char* Text(bool a_value) { return a_value ? "true" : "false"; }
}

Ini::File::File(std::filesystem::path a_path) :
	m_path(std::move(a_path))
{
	std::ifstream in{ m_path };
	if (!in) {
		return;
	}
	m_found = true;
	std::string section;
	std::string raw;
	while (std::getline(in, raw)) {
		Line       line{ raw, section, {}, {} };
		const auto text = Trim(raw);
		if (text.size() > 2 && text.front() == '[' && text.back() == ']') {
			section = text.substr(1, text.size() - 2);
			line.section = section;
		} else if (const auto eq = text.find('='); eq != std::string::npos && text.front() != ';' && text.front() != '#') {
			line.key = Trim(text.substr(0, eq));
			line.value = Trim(text.substr(eq + 1));
			if (const auto comment = line.value.find(';'); comment != std::string::npos) {
				line.value = Trim(line.value.substr(0, comment));
			}
		}
		m_lines.push_back(std::move(line));
	}
}

const std::string* Ini::File::Find(const char* a_section, const char* a_key) const
{
	for (const auto& line : m_lines) {
		if (line.key == a_key && line.section == a_section) {
			return &line.value;
		}
	}
	return nullptr;
}

bool Ini::File::Bool(const char* a_section, const char* a_key, bool a_default) const
{
	const auto* value = Find(a_section, a_key);
	if (!value || value->empty()) {
		return a_default;
	}
	const char first = static_cast<char>(std::tolower(static_cast<unsigned char>(value->front())));
	return first == 't' || first == '1' || first == 'y';
}

float Ini::File::Float(const char* a_section, const char* a_key, float a_default) const
{
	const auto* value = Find(a_section, a_key);
	if (!value) {
		return a_default;
	}
	try {
		return std::stof(*value);
	} catch (const std::exception&) {
		return a_default;
	}
}

int Ini::File::Int(const char* a_section, const char* a_key, int a_default) const
{
	return static_cast<int>(std::lround(Float(a_section, a_key, static_cast<float>(a_default))));
}

void Ini::File::Set(const char* a_section, const char* a_key, bool a_value)
{
	std::size_t sectionEnd = m_lines.size();
	bool        inSection = false;
	for (std::size_t i = 0; i < m_lines.size(); ++i) {
		auto& line = m_lines[i];
		if (line.section != a_section) {
			if (inSection) {
				sectionEnd = i;
				break;
			}
			continue;
		}
		inSection = true;
		if (line.key == a_key) {
			line.value = Text(a_value);
			line.text = std::format("{} = {}", a_key, line.value);
			return;
		}
	}
	if (!inSection) {
		m_lines.push_back({ {}, a_section, {}, {} });
		m_lines.push_back({ std::format("[{}]", a_section), a_section, {}, {} });
		sectionEnd = m_lines.size();
	}
	while (sectionEnd > 0 && Trim(m_lines[sectionEnd - 1].text).empty() && m_lines[sectionEnd - 1].section == a_section) {
		--sectionEnd;
	}
	m_lines.insert(m_lines.begin() + static_cast<std::ptrdiff_t>(sectionEnd), Line{ std::format("{} = {}", a_key, Text(a_value)), a_section, a_key, Text(a_value) });
}

bool Ini::File::Write() const
{
	std::ofstream out{ m_path, std::ios::trunc };
	if (!out) {
		return false;
	}
	for (const auto& line : m_lines) {
		out << line.text << '\n';
	}
	return static_cast<bool>(out);
}
