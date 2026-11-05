#include "PCH.h"

#include "Settings.h"

#include "utils/Logger.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <mutex>
#include <vector>

namespace settings
{
	namespace
	{
		std::string g_iniPath;
		std::mutex g_saveLock;

		std::string Lower(std::string a_text)
		{
			std::ranges::transform(a_text, a_text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_text;
		}

		std::string Trim(const std::string& a_text)
		{
			const auto first = a_text.find_first_not_of(" \t\r\n");
			if (first == std::string::npos) { return {}; }
			return a_text.substr(first, a_text.find_last_not_of(" \t\r\n") - first + 1);
		}

		std::vector<std::string> ReadLines(const std::string& a_path)
		{
			std::vector<std::string> lines;
			std::ifstream file(a_path);
			for (std::string line; std::getline(file, line);)
			{
				if (!line.empty() && line.back() == '\r') { line.pop_back(); }
				lines.push_back(line);
			}
			return lines;
		}

		// "section:key" (lower case) -> value
		std::map<std::string, std::string> ReadKeys(const std::string& a_path)
		{
			std::map<std::string, std::string> out;
			std::string section;
			for (const auto& raw : ReadLines(a_path))
			{
				const std::string line = Trim(raw);
				if (line.empty() || line[0] == ';' || line[0] == '#') { continue; }
				if (line.front() == '[' && line.back() == ']')
				{
					section = Lower(line.substr(1, line.size() - 2));
					continue;
				}
				const auto eq = line.find('=');
				if (eq == std::string::npos) { continue; }
				out[Lower(Trim(line.substr(0, eq))) + ":" + section] = Trim(line.substr(eq + 1));
			}
			return out;
		}

		void ReadUInt(const std::map<std::string, std::string>& a_keys, const std::string& a_key, std::atomic<std::uint32_t>& a_out)
		{
			const auto it = a_keys.find(a_key);
			if (it == a_keys.end()) { return; }
			try { a_out = static_cast<std::uint32_t>(std::stoul(it->second, nullptr, 0)); }
			catch (...) { logger::warn("INI {}: \"{}\" is not a number; the default stays", a_key, it->second); }
		}

		// One key's value, in the file's own line; the section and the line are added when the file has neither.
		void WriteKey(std::vector<std::string>& a_lines, const std::string& a_section, const std::string& a_key, const std::string& a_value)
		{
			const std::string wantSection = "[" + Lower(a_section) + "]";
			const std::string wantKey = Lower(a_key);
			std::size_t sectionEnd = a_lines.size();
			bool inSection = false, haveSection = false;
			for (std::size_t i = 0; i < a_lines.size(); ++i)
			{
				const std::string line = Trim(a_lines[i]);
				if (!line.empty() && line.front() == '[')
				{
					if (inSection) { sectionEnd = i; break; }
					inSection = Lower(line) == wantSection;
					haveSection = haveSection || inSection;
					continue;
				}
				if (!inSection || line.empty() || line[0] == ';') { continue; }
				const auto eq = line.find('=');
				if (eq != std::string::npos && Lower(Trim(line.substr(0, eq))) == wantKey)
				{
					a_lines[i] = a_key + "=" + a_value;
					return;
				}
			}
			if (!haveSection)
			{
				a_lines.push_back("[" + a_section + "]");
				a_lines.push_back(a_key + "=" + a_value);
				return;
			}
			// before the blank lines that end the section
			while (sectionEnd > 0 && Trim(a_lines[sectionEnd - 1]).empty()) { --sectionEnd; }
			a_lines.insert(a_lines.begin() + static_cast<std::ptrdiff_t>(sectionEnd), a_key + "=" + a_value);
		}
	}

	void Load()
	{
		g_iniPath = (std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / "MovementSpeedSlider.ini").string();
		logLevel = Defaults::logLevel;
		runPercent = Defaults::runPercent;
		walkPercent = Defaults::walkPercent;
		walkByDefault = Defaults::walkByDefault;
		if (!std::filesystem::exists(g_iniPath))
		{
			logger::warn("{} is not there; the compiled defaults are in force", g_iniPath);
			return;
		}
		const auto k = ReadKeys(g_iniPath);
		ReadUInt(k, "uloglevel:debug", logLevel);
		const auto readPercent = [&](const char* a_name, std::atomic<int>& a_out) {
			const auto it = k.find(Lower(a_name) + ":general");
			if (it == k.end()) { return false; }
			try
			{
				const int v = std::stoi(it->second);
				if (v >= kPercentMin && v <= kPercentMax) { a_out = v; }
				else { logger::warn("INI {}: {} is not {} to {}; the default stays", a_name, v, kPercentMin, kPercentMax); }
			}
			catch (...)
			{
				logger::warn("INI {}: \"{}\" is not a number; the default stays", a_name, it->second);
			}
			return true;
		};
		// The one value before Run and Walk had their own (2026-11-05) gives both their start; the next save drops its line.
		if (readPercent("iSpeedPercent", runPercent))
		{
			walkPercent = runPercent.load();
			logger::info("settings: iSpeedPercent {}% is now both the Run and the Walk value", runPercent.load());
		}
		readPercent("iRunSpeedPercent", runPercent);
		readPercent("iWalkSpeedPercent", walkPercent);
		const auto walk = k.find("bwalkbydefault:general");
		if (walk != k.end())
		{
			if (walk->second == "1") { walkByDefault = true; }
			else if (walk->second != "0") { logger::warn("INI bWalkByDefault: \"{}\" is not 0 or 1; the default stays", walk->second); }
		}
		logger::info("settings: {} read: run speed {}%, walk speed {}%, default motion {}", g_iniPath, runPercent.load(), walkPercent.load(), walkByDefault.load() ? "Walk" : "Run");
	}

	bool Save()
	{
		if (g_iniPath.empty()) { return false; }
		std::scoped_lock l(g_saveLock);
		auto lines = ReadLines(g_iniPath);
		WriteKey(lines, "Debug", "uLogLevel", std::to_string(logLevel.load()));
		std::erase_if(lines, [](const std::string& a_line) { return Lower(Trim(a_line)).starts_with("ispeedpercent"); });
		WriteKey(lines, "General", "iRunSpeedPercent", std::to_string(runPercent.load()));
		WriteKey(lines, "General", "iWalkSpeedPercent", std::to_string(walkPercent.load()));
		WriteKey(lines, "General", "bWalkByDefault", walkByDefault.load() ? "1" : "0");
		std::ofstream file(g_iniPath, std::ios::trunc);
		if (!file)
		{
			logger::warn("settings: {} could not be written", g_iniPath);
			return false;
		}
		for (const auto& line : lines) { file << line << '\n'; }
		logger::debug("settings: {} written", g_iniPath);
		return static_cast<bool>(file);
	}

	const std::string& IniPath() { return g_iniPath; }

	void ApplyLogLevel()
	{
		const auto level = logLevel.load();
		SKSE::log::set_level(static_cast<spdlog::level::level_enum>(level <= 6 ? level : 2), spdlog::level::info);
	}
}
