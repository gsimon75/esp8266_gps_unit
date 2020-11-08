<!-- Saga4 Calendar component -->
<template>

    <v-container class="s4-calendar" fluid v-touch="{
        left: (x) => mode_change(-1),
        right: (x) => mode_change(1),
        up: (x) => date_move(1),
        down: (x) => date_move(-1),
    }">
        <v-container v-if="mode === 'month'" class="s4-calendar" fluid>
            <v-row no-gutters class="s4-day-names">
                <v-col :key="c" class="s4-day-name" v-for="c in 7">
                    {{ S4Date.daynames[(weekStarts + c - 1) % 7].short }}
                </v-col>
            </v-row>
            <v-row :key="r" v-for="r in 6" no-gutters class="s4-week-of-month">
                <template v-for="c in 7">
                <template v-for="day in [nth_day_from_start((c-1) + 7*(r-1))]"> <!-- local variable -->
                <v-col :key="day.str" @click="select_day(day)" :class="get_class_for_day(day, 's4-month-day').join(' ')">
                    <slot name="day-of-month" :day="day">
                        <div class="day-of-month">{{ S4Date.twodigit(day.month + 1) }}-{{ S4Date.twodigit(day.day) }}</div>
                    </slot>
                    <slot name="month-day" :day="day">
                        <template v-if="(day.str in days) && ('events' in days[day.str])">
                            <div :class="('class' in event) ? event.class : 'event'" @click="event_clicked(event)" v-for="event in days[day.str].events" :key="event.start">
                                {{ event.title }}
                            </div>
                        </template> <!-- v-if -->
                    </slot>
                </v-col>
                </template> <!-- day -->
                </template> <!-- c -->
            </v-row>
        </v-container>

        <v-container v-else-if="mode === 'week'" class="s4-calendar" fluid>
            <v-row :key="r" v-for="r in 4" no-gutters class="s4-twodays-of-week">
                <template v-for="c in 2">
                <template v-for="delta in [weekday_delta(r, c)]"> <!-- local variable -->
                    <v-col v-if="delta == -1" :key="delta" class="s4-week-day">
                        <slot name="mini-month" :week-start="start">
                            <v-icon x-large class="ma-auto">fas fa-question</v-icon>
                        </slot>
                    </v-col>
                    <template v-else v-for="day in [nth_day_from_start(delta)]"> <!-- local variable -->
                        <v-col :key="day.str" @click="select_day(day)" :class="get_class_for_day(day, 's4-week-day').join(' ')">
                            <slot name="day-of-week" :day="day">
                                <div class="day-of-week">{{ S4Date.twodigit(day.month + 1) }}-{{ S4Date.twodigit(day.day) }} {{ S4Date.daynames[day.weekday].short }}</div>
                            </slot>
                            <slot name="week-day" :day="day">
                                <template v-if="(day.str in days) && ('events' in days[day.str])">
                                    <div :class="('class' in event) ? event.class : 'event'" @click="event_clicked(event)" v-for="event in days[day.str].events" :key="event.start">
                                        {{ event.title }}
                                        {{ S4Date.time2str(event.start, timeAmPm) }}..{{ S4Date.time2str(event.end, timeAmPm) }}
                                    </div>
                                </template> <!-- v-if -->
                            </slot>
                        </v-col>
                    </template> <!-- day -->
                </template> <!-- delta -->
                </template> <!-- c -->
            </v-row>
        </v-container>
        
        <v-container v-else-if="mode === 'day'" class="s4-calendar" fluid>
            <template v-for="day in [nth_day_from_start(0)]"> <!-- local variable -->
                <div class="s4-calendar d-flex flex-row" :key="day.str">
                    <div class="col col-8">
                        <div class="s4-day">
                            <div class="s4-day-hour" v-for="(undefined, hour) in 16" :key="hour">
                                <div class="hour-of-day">{{ S4Date.twodigit(hour + 6) }}:</div>
                            </div>
                            <slot name="solo-day" :day="day"></slot>
                            <template v-if="(day.str in days) && ('events' in days[day.str])">
                                <slot name="day-event" :day="day" :event="event" v-for="event in days[day.str].events">
                                    <div :style="day_event_position(event.start, event.end)" @click="event_clicked(event)" :class="'event-of-day ' + (('class' in event) ? event.class : 'event')" :key="event.start">
                                        {{ event.title }} {{ S4Date.time2str(event.start, timeAmPm) }}..{{ S4Date.time2str(event.end, timeAmPm) }}
                                    </div>
                                </slot>
                            </template> <!-- v-if -->
                        </div>
                    </div>
                    <div class="col col-4 blue-grey lighten-4 d-flex flex-column pa-0">
                        <div class="flex-grow-0 flex-shrink-0 pa-3">
                            <div class="text-h5">{{ S4Date.monthnames[day.month].long }}</div>
                            <div class="text-h5 red--text">{{ day.day }}</div>
                            <div class="text-body-1">{{ S4Date.daynames[day.weekday].long }}</div>
                        </div>
                        <slot name="day-info" :day="day">
                        </slot>
                    </div>
                </div>
            </template> <!-- day -->
        </v-container>
        
    </v-container>

</template>

<script>
console.log("S4Calendar.vue start");
import S4Date from "@/plugins/S4Date";

const msec_per_page = {
    "month": S4Date.ONE_DAY * 42,
    "week": S4Date.ONE_DAY * 7,
    "day" : S4Date.ONE_DAY * 1,
};
const weekday_h_delta = [ 0, 1, 2, 3, 4, 5, 6, -1];
const weekday_v_delta = [ 0, 4, 1, 5, 2, 6, 3, -1];

export default {
    name: 'S4Calendar',
    components: {
    },
    props: {
        weekendDays: {
            type: Object,
            default: () => {
                return { 0: true, 6: true };
            },
        },
        weekStarts: {
            type: Number,
            default: 0,
        },
        weekdaysHoriz: {
            type: Boolean,
            default: true,
        },
        timeAmPm: {
            type: Boolean,
            default: false,
        },
        showDay: Number,
        days: {
            type: Object,
            // { "2020-03-27": { title: "Workout", class: "workout-day", events: [ { title: ..., class: ..., start: 60*11, end: 60*11 + 30}, ... ] }, ... }
            default: () => {
                return {}
            },
        },
        modes: {
            type: Array,
            default: () => {
                return ["month", "week", "day"];
            },
        },
    },
    data: function () {
        const now = new Date();
        const today_utc = Date.UTC(now.getFullYear(), now.getMonth(), now.getDate());
        const today = new S4Date(today_utc);
        return {
            S4Date,
            today: today, // TODO: refresh periodically
            mode_idx: 0,
            start: today_utc,
            selected_day: today,
        };
    },
    computed: {
        mode: function () {
            return this.modes[this.mode_idx];
        },
        end: function () {
            return this.start + msec_per_page[this.mode];
        },
    },
    methods: {
        debug: function(msg, x) {
            console.log("" + msg + JSON.stringify(x));
            return "";
        },
        go_to_day: function (timestamp, force=false) {
            const day = new Date(timestamp);
            var start_day, start_day_wk;
            
            // months must align the 1st day on the top line
            if (this.mode == "month") {
                start_day = Date.UTC(day.getFullYear(), day.getMonth(), 1);
                start_day_wk = new Date(start_day).getDay();
            }
            else {
                start_day = Date.UTC(day.getFullYear(), day.getMonth(), day.getDate());
                start_day_wk = day.getDay();
            }
            
            // multi-day views must start on a "weekStarts" day
            if (this.mode != "day") {
                var days_to_rewind = (start_day_wk - this.weekStarts + 7) % 7;
                start_day = start_day - S4Date.ONE_DAY * days_to_rewind;
            }

            if ((this.start != start_day) || force) {
                this.start = start_day;
                this.$emit("date-change", { start: this.start, end: this.end });
            }
        },
        select_day: function (day, force=false) {
            if ((this.selected_day.str != day.str) || force) {
                if ((this.selected_day.year != day.year) || (this.selected_day.month != day.month)) {
                    this.go_to_day(day.timestamp);
                }
                this.selected_day = day;
                this.$emit("day-selected", this.selected_day);
            }
        },
        nth_day_from_start: function(n) {
            return new S4Date(this.start + S4Date.ONE_DAY * n);
        },
        day_event_position: function(start, end) {
            // 0% = 6:00, 100% = 22:00 -> total length = 16 hours
            let len = end - start;
            start -= 6 * 60;
            return "top: " + (start * 100 / (16*60)) + "%; height: " + (len * 100 / (16*60)) + "%;";
        },
        event_clicked: function(e) {
            this.$emit("event-selected", e);
        },
        get_class_for_day: function(day, ...base_classes) {
            var classes = base_classes;

            if (day.same_day(this.selected_day)) {
                classes.push("selected-day");
            }
            if (day.same_day(this.today)) {
                classes.push("today");
            }
            else if (day.same_month(this.today)) {
                classes.push("this-month");
            }
            else {
                classes.push("unrelated");
            }

            // add a class for workday or weekend
            classes.push((day.weekday in this.weekendDays) ? "weekend" : "workday");

            // add custom class(es) if any
            if ((day.str in this.days) && ('class' in this.days[day.str])) {
                const c = this.days[day.str].class;
                if (Array.isArray(c)) {
                    classes.push(...c);
                }
                else {
                    classes.push(c);
                }
            }

            return classes.filter(c => !!c);
        },
        weekday_delta: function(row, col) {
            return (this.weekdaysHoriz ? weekday_h_delta : weekday_v_delta)[(row-1) * 2 + (col-1)];
        },
        mode_change: function(delta) {
            this.mode_idx = (this.mode_idx + delta + this.modes.length) % this.modes.length;
            this.go_to_day(this.selected_day.timestamp);
        },
        date_move: function (delta) {
            if (delta === 0) {
                return;
            }
            switch (this.mode) {
                case "month": {
                    // go to the previous/next month and select the day of the same cell

                    // store the selection position compared to the start
                    const delta_selected_from_start = this.selected_day.timestamp - this.start;

                    // calculate the new start date
                    var year = this.selected_day.year;
                    var month = this.selected_day.month + delta;
                    if (month < 0) {
                        year += 0|((month - 11) / 12);
                        month = 12 + (month % 12);
                    }
                    else if (12 <= month) {
                        year += 0|(month / 12);
                        month = month % 12;
                    }

                    // set the new start date
                    this.go_to_day(Date.UTC(year, month, 1));

                    // restore selected position
                    this.select_day(new S4Date(this.start + delta_selected_from_start));
                    break;
                }
                
                case "week": {
                    // go to the previous/next month and select the day of the same cell

                    // store the selection position compared to the start
                    const delta_selected_from_start = this.selected_day.timestamp - this.start;

                    // set the new start date
                    this.go_to_day(this.start + delta * 7*S4Date.ONE_DAY); 

                    // restore selected position
                    this.select_day(new S4Date(this.start + delta_selected_from_start));
                    break;
                }
                
                case "day": {
                    // set the new start date
                    this.go_to_day(this.start + delta * S4Date.ONE_DAY); 
                    // restore selected position
                    this.select_day(new S4Date(this.start));
                    break;
                }
            }
        },
    },
    mounted: function () {
        if (this.showDay) {
            this.go_to_day(this.showDay, true);
            this.select_day(new S4Date(this.showDay, true));
        }
        else {
            this.go_to_day(this.today.timestamp, true);
            this.select_day(this.today, true);
        }
    },
    beforeDestroy: function () {
        this.$emit("day-selected", null);
    },
}
</script>

<style lang="scss">
@import "~vuetify/src/styles/main.sass";

    .s4-calendar {
        @extend .pa-0;
        height: 100%;
    }

    /* ---- month ---- */

    .s4-day-names {
        @extend .white--text, .text-caption, .py-0, .my-0;
        background-color: map-get($blue, "darken-1");
        height: 4%;
    }

    .s4-day-name {
        @extend .py-0, .my-0;
    }

    .s4-week-of-month {
        height: 16%;
    }

    .s4-month-day {
        @extend .d-flex, .flex-column, .text-caption, .pa-0;
        border-bottom: 1px solid map-get($grey, "base");
        position: relative;
        overflow: hidden;
    }

    .s4-month-day .day-of-month {
        @extend .text-subtitle-1;
    }

    .s4-month-day .event {
        background-color: map-get($amber, "lighten-2");
        white-space: nowrap;
    }

    /* ---- week ---- */

    .s4-twodays-of-week {
        height: 25%;
    }

    .s4-week-day {
        display: flex;
        flex-direction: column;
        font-size: 0.9rem;
        padding: 0;
        position: relative;
        overflow: hidden;
        border-style: solid;
        border-color: grey;
        border-width: 0 1px 1px 0;
    }

    .s4-week-day .day-of-week {
        @extend .text-subtitle-1;
    }

    .s4-week-day .event {
        background-color: map-get($amber, "lighten-2");
        white-space: nowrap;
    }

    /* ---- day ---- */

    .s4-day-hour {
        display: block;
        border-top: 1px dotted map-get($grey, "lighten-2");
        background-color: white;
        flex-grow: 1;
    }

    .hour-of-day {
        text-align: left;
    }
    
    .event-of-day {
        position: absolute;
        right: 2rem;
        width: calc(100% - 5em);
        /* top and height must be set by other source! */
        font-size: 0.9rem;
    }

    .s4-day {
        position: relative;
        display: flex;
        flex-direction: column;
        height: 100%;
    }

    .s4-day .event {
        background: linear-gradient(white, map-get($red, "lighten-4"));
        border-color: map-get($red, "lighten-1");
        border-style: solid;
        border-width: 1px 1px 1px 8px;
        color: black;
    }

    /* ---- common ---- */

    .workday {
        background-color: map-get($grey, "lighten-2");
        color: map-get($cyan, "base");
    }
    .weekend {
        background-color: map-get($grey, "lighten-1");
        color: map-get($cyan, "lighten-4");
    }

    .workday.this-month {
        background-color: white;
        color: map-get($cyan, "lighten-1");
    }
    .weekend.this-month {
        background-color: map-get($grey, "lighten-4");
        color: map-get($cyan, "lighten-1");
    }

    .workday.today {
        background-color: map-get($blue-grey, "darken-1");
        color: white;
    }
    .weekend.today {
        background-color: map-get($blue-grey, "darken-2");
        color: white;
    }

    .selected-day {
        background-color: map-get($lime, "lighten-1") !important;
        color:black;
    }
</style>
// vim: set sw=4 ts=4 indk= et:
