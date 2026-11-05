#pragma once

#include <atomic>
#include <cstdint>
#include <string>

// The mod's settings: read from SKSE\Plugins\MovementSpeedSlider.ini, written back line by line (never with
// WritePrivateProfileString, which reports success and writes nothing under MO2's virtual folder). The journal and DevBench run on
// different threads, so every value is atomic. Compiled defaults match the shipped INI (rule 16).
namespace settings
{
	struct Defaults
	{
		static constexpr std::uint32_t logLevel = 2;
		static constexpr int speedPercent = 100;
	};

	inline constexpr int kPercentMin = 25;
	inline constexpr int kPercentMax = 150;

	inline std::atomic<std::uint32_t> logLevel{ Defaults::logLevel };
	inline std::atomic<int> speedPercent{ Defaults::speedPercent };  // iSpeedPercent:General - the slider

	void Load();  // at plugin load; a missing INI or line keeps the default
	bool Save();  // every value, keeping the file's comments; false when the file cannot be written. Any thread.
	const std::string& IniPath();
	void ApplyLogLevel();
}
