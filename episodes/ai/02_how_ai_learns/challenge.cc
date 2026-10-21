// Copyright 2026 Cognitas AI Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

namespace hotpath::episode02 {

/**
 * @brief Audits the Day-1 Clone Symmetry Trap vs Kaiming Initialization and
 *        demonstrates the 99% Accuracy Paradox on imbalanced data.
 */
void RunEpisode02Audit() {
  std::cout << "==============================================================="
               "=========\n";
  std::cout << "  HOT PATH C++ // EPISODE 02: HOW AI ACTUALLY LEARNS AUDIT\n";
  std::cout << "==============================================================="
               "=========\n\n";

  // 1. The Sleeping Neuron Alibi (Chain Rule: dL/dw = delta * a_in)
  const float delta_error = 0.991f;  // 99.1% Toaster mistake!
  const float a_sleeping = 0.0f;
  const float a_screaming = 8.5f;
  std::cout << "[1] THE SLEEPING NEURON ALIBI (dL/dw = delta * a_in):\n";
  std::cout << "  Upstream Error Signal (delta) : +" << std::fixed
            << std::setprecision(3) << delta_error << "\n";
  std::cout << "  Sleeping Neuron (a_in = 0.0)  : Blame = "
            << (delta_error * a_sleeping) << " (100% Alibi -> Zero Update!)\n";
  std::cout << "  Screaming Neuron (a_in = 8.5) : Blame = +"
            << (delta_error * a_screaming) << " (Takes Full Blame!)\n\n";

  // 2. The 99% Accuracy Lie: bool is_cracked() { return false; }
  const int total_blades = 1000;
  const int intact_blades = 990;
  const int cracked_blades = 10;
  const int tp = 0;
  const int fp = 0;
  const int tn = intact_blades;
  const int fn = cracked_blades;

  const double accuracy =
      100.0 * static_cast<double>(tp + tn) / static_cast<double>(total_blades);
  const double recall =
      100.0 * static_cast<double>(tp) / static_cast<double>(tp + fn);
  (void)fp;

  std::cout << "[2] WHEN 99% ACCURACY IS A LIE (bool is_cracked() { return "
               "false; }):\n";
  std::cout << "  Dataset : " << intact_blades << " Intact Turbine Blades, "
            << cracked_blades << " Cracked Blades\n";
  std::cout << "  Raw Accuracy : " << std::setprecision(1) << accuracy
            << "% (Looks like an A+ on paper!)\n";
  std::cout << "  True Recall  : " << recall
            << "% (Missed 10 out of 10 cracked blades!)\n";
  std::cout << "==============================================================="
               "=========\n";
}

}  // namespace hotpath::episode02

int main() {
  hotpath::episode02::RunEpisode02Audit();
  return 0;
}
