static const char* delim = " ";

static const auto modules = std::to_array<std::unique_ptr<Module>> ({
	std::make_unique< Thinklight  >(),
	std::make_unique< MPD         >(),
	std::make_unique< WIFI        >(),
	std::make_unique< Temperature >(),
	std::make_unique< Battery     >(),
	std::make_unique< Memory      >(),
	std::make_unique< DateTime    >()
});
