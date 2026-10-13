#ifndef __PERFORMANCEGUI_H
#define __PERFORMANCEGUI_H

void screenTools();
void performanceLab();
// Publish the one-time cold-start measurement captured by main.cpp. It is
// retained for the performance report and does not change normal UI state.
void performanceRecordStartupMilliseconds(int milliseconds);

#endif
