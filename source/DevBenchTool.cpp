#include "PCH.h"

#include "DevBenchTool.h"

#include "DevBench/DevBenchAPI.h"
#include "Row.h"
#include "Settings.h"
#include "Speed.h"
#include "utils/Logger.h"

#include <format>
#include <string>
#include <string_view>

// DevBench tool "msp.control" (rules 31 and 64): the row and the speed's live state, and setting the value with no key pressed. A real
// test still moves the real slider. English only: only a tool reads this (rule 66).
namespace DevBenchTool
{
	namespace
	{
		// The value of a top-level JSON member, as text: a quoted string or a bare word. Empty when absent.
		std::string Get(std::string_view a_json, const char* a_name)
		{
			const std::string key = std::format("\"{}\"", a_name);
			auto pos = a_json.find(key);
			if (pos == std::string_view::npos) { return {}; }
			pos = a_json.find(':', pos + key.size());
			if (pos == std::string_view::npos) { return {}; }
			++pos;
			while (pos < a_json.size() && (a_json[pos] == ' ' || a_json[pos] == '\t')) { ++pos; }
			if (pos >= a_json.size()) { return {}; }
			if (a_json[pos] == '"')
			{
				const auto close = a_json.find('"', pos + 1);
				return close == std::string_view::npos ? std::string{} : std::string(a_json.substr(pos + 1, close - pos - 1));
			}
			std::string out;
			while (pos < a_json.size() && a_json[pos] != ',' && a_json[pos] != '}' && a_json[pos] != ' ') { out += a_json[pos++]; }
			return out;
		}

		void ControlTool(void*, const char* a_args, void* a_sink, DevBenchAPI::WriteFn a_write)
		{
			const std::string_view args = a_args ? a_args : "";
			const std::string op = Get(args, "op");
			std::string ok = "true";
			if (op == "set")
			{
				try { speed::SetPercent(std::stoi(Get(args, "value"))); }
				catch (...) { ok = "false,\"error\":\"value is a whole number, 25 to 150\""; }
			}
			else if (!op.empty() && op != "state") { ok = "false,\"error\":\"op is state or set\""; }

			speed::SampleMovementSpeed();
			const auto stats = speed::GetStats();
			const auto state = row::GetState();
			const std::string out = std::format(
				R"({{"ok":{},"op":"{}","percent":{},"hookInstalled":{},"hookCount":{},"speedMultBase":{},"speedMultApplied":{},"movementSpeed":{},"rowPresent":{},"rowIndex":{},"rowClip":{},"listEntries":{},"entryValue":{},"listPath":"{}"}})",
				ok, op.empty() ? "state" : op, settings::speedPercent.load(), stats.installed, stats.hookCount, stats.lastBase, stats.lastApplied, stats.movementSpeed,
				state.present, state.index, state.clip, state.entries, state.entryValue, state.listPath);
			a_write(a_sink, out.c_str());
		}
	}

	void Init(bool a_lastAttempt)
	{
		static bool registered = false;
		if (registered) { return; }
		DevBenchAPI::IDevBenchInterface001* devBench = DevBenchAPI::GetDevBenchInterface001();
		if (!devBench)
		{
			if (a_lastAttempt) { logger::info("DevBench not detected; skipping the \"msp.control\" tool"); }
			return;
		}
		constexpr const char* descriptor =
			"{"
			"\"description\":\"Movement Speed Slider live state and driver. op=state (default): percent, hook installed and read count, SpeedMult base and applied at the latest read, the player's movement speed sampled at the previous call, the row in the journal's list. op=set value=25..150: sets the percent as the slider would, saves the INI.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"op\":{\"type\":\"string\"},\"value\":{\"type\":\"number\"}}},"
			"\"readOnly\":false"
			"}";
		if (devBench->RegisterTool("msp.control", descriptor, &ControlTool, nullptr))
		{
			logger::info("registered \"msp.control\" with DevBench (build {})", devBench->GetBuildNumber());
			registered = true;
		}
	}
}
