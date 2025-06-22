#include "Utils/Timestep.hpp"

#include "Utils/Timer.hpp"

namespace ox {
Timestep::Timestep() : timestep(0.0), last_time(0.0), elapsed(0.0) { timer = new Timer(); }

Timestep::~Timestep() { delete timer; }

void Timestep::on_update(this Timestep& self) {
  ZoneScoped;

  f64 current_time = self.timer->get_elapsed_msd();
  f64 dt = current_time - self.last_time;

  {
    ZoneNamedN(z, "Sleep TimeStep to target fps", true);
    while (dt < self.max_frame_time) {
      current_time = self.timer->get_elapsed_msd();
      dt = current_time - self.last_time;
    }
  }

  self.timestep = current_time - self.last_time;
  self.last_time = current_time;
  self.elapsed += self.timestep;
}
} // namespace ox
