/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM power_meter

#if !defined(_TRACE_POWER_METER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_POWER_METER_H

#include <linux/tracepoint.h>

TRACE_EVENT(power_meter,
	TP_PROTO(int meter_id, u64 timestamp, u64 ch0, u64 ch1, u64 ch2,
		 u64 ch3, u64 ch4, u64 ch5, u64 ch6, u64 ch7),
	TP_ARGS(meter_id, timestamp, ch0, ch1, ch2, ch3, ch4, ch5, ch6, ch7),
	TP_STRUCT__entry(
		__field(int, meter_id)
		__field(u64, timestamp)
		__field(u64, ch0)
		__field(u64, ch1)
		__field(u64, ch2)
		__field(u64, ch3)
		__field(u64, ch4)
		__field(u64, ch5)
		__field(u64, ch6)
		__field(u64, ch7)
	),
	TP_fast_assign(
		__entry->meter_id = meter_id;
		__entry->timestamp = timestamp;
		__entry->ch0 = ch0;
		__entry->ch1 = ch1;
		__entry->ch2 = ch2;
		__entry->ch3 = ch3;
		__entry->ch4 = ch4;
		__entry->ch5 = ch5;
		__entry->ch6 = ch6;
		__entry->ch7 = ch7;
	),

	TP_printk("meter_id=%d timestamp=%llu ch0=%llu ch1=%llu ch2=%llu ch3=%llu ch4=%llu ch5=%llu ch6=%llu ch7=%llu",
		  __entry->meter_id, __entry->timestamp, __entry->ch0, __entry->ch1, __entry->ch2,
		  __entry->ch3, __entry->ch4, __entry->ch5, __entry->ch6, __entry->ch7)
);
#endif /* _TRACE_POWER_METER_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
