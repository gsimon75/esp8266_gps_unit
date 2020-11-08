<template>
    <div class="calendar fill-height">
        <s4-calendar :days="days" @date-change="get_events" @day-selected="day_selected" @event-selected="event_clicked">

            <!--
            Displaying a day in month view
            -->
            <template v-slot:month-day="{day}">
                <template v-if="(day.str in days) && ('events' in days[day.str])">
                    <div :class="('class' in event) ? event.class : 'event'" @click="event_clicked(event)" v-for="event in days[day.str].events" :key="event.start">
                        {{ event.title }}
                    </div>
                </template> <!-- v-if -->
                <div class="flex-grow-1"></div>
                <v-btn fab color="green" class="month-day-badge">{{ num_free_session_slots(day) }}</v-btn>
            </template>

            <!--
            Displaying a day in week view
            -->
            <template v-slot:week-day="{day}">
                <div class="timeslot-bar">
                    <template v-for="(timeslot_class, idx) in free_session_slots(day)">
                        <div :key="idx" :class="'week-timeslot ' + timeslot_class"></div>
                    </template>
                </div>
                <template v-if="(day.str in days) && ('events' in days[day.str])">
                    <div :class="('class' in event) ? event.class : 'event'" @click="event_clicked(event)" v-for="event in days[day.str].events" :key="event.start">
                        {{ event.title }}
                        {{ S4Date.time2str(event.start) }}..{{ S4Date.time2str(event.end) }}
                    </div>
                </template> <!-- v-if -->
            </template>

            <!--
            Displaying a day in day view
            -->
            <template v-slot:solo-day="">
                <div class="timeslot-column">
                    <template v-for="(timeslot_class, idx) in free_session_slots(selected_day)">
                        <div :key="idx" :class="'day-timeslot ' + timeslot_class"></div>
                    </template>
                </div>
            </template>

            <!--
            Displaying a side bar in day view
            -->
            <!--template v-slot:day-info>
                <div class="d-flex flex-column flex-grow-1">
                    <div v-for="n in ['energy', 'protein', 'carbs', 'fat', 'water']" :key="n" :class="'day-meter-row background-' + n">
                        <s4-progress class="day-meter-gauge" :title="nutrient_labels[n]" :low="day_nutrition_stats[n].low" :high="day_nutrition_stats[n].high" :value="day_nutrition_stats[n].value"></s4-progress>
                    </div>
                </div>
            </template-->
        </s4-calendar>

        <v-btn color="primary" @click="start_booking" fab dark small style="position: absolute; bottom: 2.5%; right: 8.5%">
          <v-icon>fas fa-plus</v-icon>
        </v-btn>

        <v-bottom-sheet v-model="choosing_trainers" scrollable>
            <template v-slot:activator="{ on, attrs }">
                <v-btn v-bind="attrs" v-on="on" color="primary" fab dark small style="position: absolute; bottom: 2.5%; right: 23%">
                  <v-icon>fas fa-user-friends</v-icon>
                </v-btn>
            </template>
            <v-card>
                <v-card-title>Choose trainers</v-card-title>
                <v-card-text style="height: 60vh;">
                    <v-list-item-group v-model="selected_trainers" color="primary" multiple>
                        <v-list-item v-for="trainer in S4DataSource.trainers" :key="trainer.name" :value="trainer.name">
                            <template v-slot:default="{ active }">
                                <v-list-item-action>
                                    <v-checkbox
                                        :input-value="active"
                                        color="deep-purple accent-4"
                                    ></v-checkbox>
                                </v-list-item-action>
                                <v-list-item-content>{{ trainer.name }}</v-list-item-content>
                            </template>
                        </v-list-item>
                    </v-list-item-group>
                </v-card-text>
            </v-card>
        </v-bottom-sheet>

        <!--
        Details of existing event with option to unbook it
        -->
        <v-dialog v-model="showing_event_details" max-width="90vw" modal>
            <v-card v-if="!!selected_event">
                <v-card-title class="d-block text-h4">{{ selected_event.title }}</v-card-title>

                <v-card-title class="d-block text-h5">
                    {{ S4Date.time2str(selected_event.start) }}..{{ S4Date.time2str(selected_event.end) }}
                </v-card-title>

                <v-card-title class="d-block text-h5">
                    with {{ selected_event.trainer }}
                </v-card-title>

                <v-divider></v-divider>
                <v-card-actions>
                    <v-row justify="center">
                        <v-btn color="error" @click="unbook_event(selected_event)" rounded><v-icon x-small left>fas fa-calendar-minus</v-icon> UnBook</v-btn>
                    </v-row>
                </v-card-actions>

            </v-card>
        </v-dialog>

        <!--
        Alert for mocking the unbook step
        -->
        <v-dialog v-model="show_event_unbooked" max-width="60vw">
            <v-card>
                <v-card-title>
                    Event unbooked
                </v-card-title>
                <v-card-actions>
                    <v-row justify="center">
                        <v-btn color="success" @click="show_event_unbooked = false">OK</v-btn>
                    </v-row>
                </v-card-actions>
            </v-card>
        </v-dialog>

        <!--
        Input form for booking new session
        -->
        <v-dialog v-model="showing_event_booking" max-width="90vw" modal>
            <v-card v-if="!!selected_day">
                <v-card-title class="d-block text-h4">{{ selected_day.str }}</v-card-title>

                <v-card-title class="d-block text-h5">Book new session</v-card-title>
                <v-divider></v-divider>

                <v-container>
                    <v-row justify="center"><v-col>
                        <v-combobox v-model="new_event.title" :items="S4DataSource.workout_types" label="of" placeholder="<session type>" outlined dense></v-combobox>
                    </v-col></v-row>
                    <v-row justify="center"><v-col>
                        <v-combobox v-model="new_event.start" :items="timeslots_bookable" label="from" placeholder="<time>" outlined dense></v-combobox>
                    </v-col></v-row>
                    <v-row justify="center"><v-col>
                        <v-combobox v-model="new_event.trainer" :items="trainers_bookable" label="with" placeholder="<trainer>" outlined dense></v-combobox>
                    </v-col></v-row>
                    <v-row justify="center"><v-col>
                         <v-checkbox v-model="new_event.is_recurring" label="Recurring" ></v-checkbox>
                    </v-col></v-row>
                    <v-row justify="center"><v-col>
                        <v-btn color="error" @click="book_event" rounded><v-icon x-small left>fas fa-calendar-plus</v-icon> Book</v-btn>
                    </v-col></v-row>
                </v-container>

            </v-card>
        </v-dialog>

        <!--
        Alert for mocking the book step
        -->
        <v-dialog v-model="show_event_booked" max-width="60vw">
            <v-card>
                <v-card-title>
                    Event booked
                </v-card-title>
                <v-card-actions>
                    <v-row justify="center">
                        <v-btn color="success" @click="show_event_booked = false">OK</v-btn>
                    </v-row>
                </v-card-actions>
            </v-card>
        </v-dialog>

    </div>
</template>

<script>
import S4Calendar from "@/components/S4Calendar.vue"
//import S4Progress from "@/components/S4Progress.vue"
import S4DataSource from "@/plugins/S4DataSource"
import S4Date from "@/plugins/S4Date"

export default {
    name: "CalendarPage",
    components: {
        S4Calendar,
        //S4Progress,
    },
    data() {
        return {
            S4Date,
            S4DataSource,
            time_iv: {},
            days: {},
            day_nutrition_stats: {},
            nutrient_labels: { // NOTE: will be localized anyway
                energy: "Energy",
                protein: "Protein",
                carbs: "Carbs",
                fat: "Fat",
                water: "Water",
            },

            choosing_trainers: false,
            selected_trainers: [],
            trainers_available: {},

            showing_event_details: false,
            selected_event: null,

            show_event_unbooked: false,

            showing_event_booking: false,
            trainers_bookable: ["Arun", "Shaphic"],
            timeslots_bookable: ["09:30", "10:00", "12:00", "17:30"],
            selected_day: null,
            new_event: {
                title: "",
                trainer: "",
                start: 0,
                end: 0,
                is_recurring: false,
            },

            show_event_booked: false,
        };
    },
    watch: {
        selected_trainers: function () {
            this.trainers_available = S4DataSource.get_trainers_available(this.time_iv, this.selected_trainers);
            console.log("selection changed, trainers_available=" + this.trainers_available);
        },
    },
    methods: {
        debug(x) {
            console.log("debug: " + JSON.stringify(x));
            return "y";
        },
        get_events(iv) {
            this.time_iv = {...iv};
            this.days = S4DataSource.get_events(iv);
            this.trainers_available = S4DataSource.get_trainers_available(iv, this.selected_trainers);
            console.log("date changed, trainers_available=" + this.trainers_available);
        },
        num_free_session_slots (day) {
            console.log("Trainers for " + day.str + ": " + JSON.stringify(this.trainers_available[day.str]));
            if (!this.trainers_available || !(day.str in this.trainers_available)) {
                return 0;
            }
            return Object.keys(this.trainers_available[day.str]).length;
        },
        free_session_slots (day) {
            if (!this.trainers_available || !(day.str in this.trainers_available)) {
                return [ "timeslot-unavailable" ];
            }
            var result = [];
            console.log("Trainers for " + day.str + ": " + JSON.stringify(this.trainers_available[day.str]));
            for (let start = 6.0; start < 22.0; start += 0.5) {
                if (start in this.trainers_available[day.str]) {
                    result.push("timeslot-available");
                }
                else {
                    result.push("timeslot-unavailable");
                }
            }
            console.log("Free slots for " + day.str + ": " + JSON.stringify(result));

            return result;
        },
        timeslot_class: function (hour) {
            return (hour in this.trainers_available[this.selected_day.str]) ? "timeslot-available" : "timeslot-unavailable";
        },
        event_clicked: function (e) {
            console.log("event_clicked: " + e);
            this.selected_event = e;
            this.showing_event_details = true;
        },
        unbook_event: function (e) {
            console.log("unbook_event: " + e);
            this.show_event_unbooked = true;
        },
        day_selected: function (day) {
            if (day === null) {
                this.$store.state.app_bar_info = "...";
            }
            else {
                this.$store.state.app_bar_info = day.str;
                this.selected_day = day;
                //this.day_nutrition_stats = S4DataSource.get_nutrition_stats({start: day.timestamp, end: day.timestamp})[day.str];
            }
        },
        start_booking: function () {
            this.new_event.title = null;
            this.new_event.start = null;
            this.new_event.trainer = null;
            this.new_event.is_recurring = false;
            this.showing_event_booking = true;
        },
        book_event: function (e) {
            console.log("book_event: " + e);
            this.showing_event_booking = false;
            this.show_event_booked = true;
        },
    },
}
</script>

<style lang="scss">
@import "~vuetify/src/styles/main.sass";

    .v-row {
        margin-bottom: 20px;
        &:last-child {
            margin-bottom: 0;
        }
    }

    .service-hours {
        @extend .orange, .lighten-4;
        border: solid 1px orange;
        color: black;
    }

    .s4-calendar .normal-day .day-of-month,
    .s4-calendar .normal-day .day-of-week {
        background: linear-gradient(map-get($blue, "lighten-4"), map-get($blue, "lighten-2"));
        border-color: map-get($blue, "base");
        border-style: solid;
        border-width: 1px 1px 0 1px;
        color: black;
    }
    .s4-calendar .normal-day .timeslot-unavailable {
        background-color: map-get($blue, "lighten-3");
    }
    .s4-calendar .normal-day .timeslot-available {
        background-color: map-get($blue, "lighten-4");
    }

    .s4-calendar .cheat-day .day-of-month,
    .s4-calendar .cheat-day .day-of-week {
        background: linear-gradient(map-get($amber, "lighten-4"), map-get($amber, "lighten-2"));
        border-color: map-get($amber, "base");
        border-style: solid;
        border-width: 1px 1px 0 1px;
        color: black;
    }
    .s4-calendar .cheat-day .timeslot-unavailable {
        background-color: map-get($amber, "lighten-3");
    }
    .s4-calendar .cheat-day .timeslot-available {
        background-color: map-get($amber, "lighten-4");
    }
    
    .s4-calendar .workout-day .day-of-month,
    .s4-calendar .workout-day .day-of-week {
        background: linear-gradient(map-get($red, "lighten-4"), map-get($red, "lighten-2"));
        border-color: map-get($red, "base");
        border-style: solid;
        border-width: 1px 1px 0 1px;
        color: white;
    }
    .s4-calendar .workout-day .timeslot-unavailable {
        background-color: map-get($red, "lighten-3");
    }
    .s4-calendar .workout-day .timeslot-available {
        background-color: map-get($red, "lighten-4");
    }

    .s4-calendar .s4-month-day .workout {
        background: linear-gradient(map-get($red, "lighten-2"), map-get($red, "lighten-1"));
        border-color: map-get($red, "base");
        border-style: solid;
        border-width: 0 1px 1px 1px;
        color: black;
    }

    .s4-calendar .s4-week-day .workout {
        background: linear-gradient(map-get($red, "lighten-2"), map-get($red, "lighten-1"));
        border-color: map-get($red, "base");
        white-space: nowrap;
        border-style: solid;
        border-width: 0 1px 1px 0;
        color: black;
    }

    .s4-calendar .s4-day .workout {
        background: linear-gradient(map-get($red, "lighten-4"), map-get($red, "lighten-2"));
        border-color: map-get($red, "base");
        border-style: solid;
        border-width: 1px 1px 1px 8px;
        color: black;
    }

    .s4-day .timeslot-unavailable {
        background-color: map-get($green, "lighten-3");
    }
    .s4-day .timeslot-available {
        background-color: map-get($green, "lighten-4");
    }

    .day-meter-row {
        display: flex;
        flex-direction: column;
        flex-grow: 1;
        flex-shrink: 1;
        padding-top: 0.3rem;
        padding-bottom: 0.3rem;
    }

    .day-meter-gauge {
        height: 0;
        flex-grow: 1;
        flex-shrink: 1;
    }

    .month-day-badge {
        width: 16px !important;
        height: 16px !important;
        font-size: 0.625rem !important;
        align-self: flex-end;
        margin: 3px;
    }

    .timeslot-bar {
        display: flex;
        flex-direction: row;
        height: 0.5rem;
    }

    .timeslot-column {
        position: absolute;
        top: 0;
        bottom: 0;
        right: 0;
        width: 1em;
        display: flex;
        flex-direction: column;
    }

    .week-timeslot {
        flex-grow: 1;
        width: 0;
    }

    .day-timeslot {
        flex-grow: 1;
        width: 100%;
        height: 0;
        margin: 1px 0 1px 0;

    }

</style>

// vim: set sw=4 ts=4 et indk= :
