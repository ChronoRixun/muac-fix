#pragma once
namespace dpi_awareness {
    // Called from DllMain (process attach), before the game creates any window.
    void apply();
    // Logs what apply() did, once the log is safe to write (runtime::install).
    void report();
}
