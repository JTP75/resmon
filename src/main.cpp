#include <iostream>

#include "resmon/config.hpp"

// Sampler/publisher wiring lands in a later commit; for now this just
// validates configuration so the config module is exercisable end to end.
int main(int argc, char** argv) {
  try {
    resmon::Config cfg = resmon::parseConfig(argc, argv);
    std::cout << "resmon configured:\n"
              << "  llama_url:   " << cfg.llama_url << "\n"
              << "  mqtt:        " << cfg.mqtt_scheme << "://" << cfg.mqtt_host << ":"
              << cfg.resolvedMqttPort() << "\n"
              << "  state topic: " << cfg.stateTopic() << "\n"
              << "  status topic:" << cfg.statusTopic() << "\n"
              << "  interval:    " << cfg.interval_seconds << "s\n";
    return 0;
  } catch (const resmon::HelpRequested&) {
    std::cout << resmon::usageText();
    return 0;
  } catch (const resmon::ConfigError& e) {
    std::cerr << "error: " << e.what() << "\n\n" << resmon::usageText();
    return 1;
  }
}
