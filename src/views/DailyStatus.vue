<template>
    <div class="daily-status d-flex flex-column">

        <div v-for="n in nutrients" :key="n" :class="'nutrient-row background-' + n" @click="show_details(n)"> 
            <div class="col col-8">
                <apexchart height="100%" :options="nutrition_data[n].options" :series="nutrition_data[n].stats"></apexchart>
            </div>
            <div class="col-4 gauge-box">
                <s4-progress :title="nutrient_labels[n]" :unit="nutrient_units[n]" :low="day_nutrition_stats[n].low" :high="day_nutrition_stats[n].high" :value="day_nutrition_stats[n].value"></s4-progress>
            </div>
        </div>

    <v-dialog v-model="showing_detailed_chart" width="100%">
        <v-card v-touch="{
            left: (x) => details_step(-1),
            right: (x) => details_step(1),
            up: (x) => details_mode(),
            down: (x) => details_mode(),
        }">
            <v-card-title class="headline">
                {{ nutrient_labels[detailed_chart_nutrient] }}
            </v-card-title>

            <apexchart :options="detailed_chart_data.options" :series="detailed_chart_data.stats"></apexchart>
        </v-card>
    </v-dialog>

    </div>

</template>

<script>

import colors from "vuetify/lib/util/colors";
import S4Progress from "@/components/S4Progress.vue";
import S4Date from "@/plugins/S4Date";
import S4DataSource from "@/plugins/S4DataSource";
import VueApexCharts from "vue-apexcharts";

export default {
    name: "DailyStatusPage",
    components: {
        S4Progress,
        "apexchart": VueApexCharts,
    },
    data() {
        return {
            nutrients: ["energy", "protein", "carbs", "fat", "water"],
            nutrient_labels: { // NOTE: will be localized anyway
                energy: "Energy",
                protein: "Protein",
                carbs: "Carbs",
                fat: "Fat",
                water: "Water",
            },
            nutrient_units: {
                energy: "kcal",
                protein: "g",
                carbs: "g",
                fat: "g",
                water: "ml",
            },
            num_hgrid: 4,
            raw_nutrition_data: [],
            nutrition_data: {},

            showing_detailed_chart: false,
            detailed_chart_nutrient: "",
            detailed_chart_data: {},
            detailed_chart_mode: "week",
            detailed_chart_day: new S4Date(),
        };
    },
    computed: {
        day_nutrition_stats: function () {
            const day = new S4Date();
            return this.raw_nutrition_data[day.str];
        },
    },
    methods: {
        get_nutrition_stats: function (all_stats, n) {
            var nmax = 0;
            var nstats = [
                {
                    name: "low",
                    type: "area",
                    data: all_stats.map(x => [x.timestamp, x[n].low]),
                },
                {
                    name: "value",
                    type: "line",
                    data: all_stats.map(x => {
                        if (nmax < x[n].value) {
                            nmax = x[n].value;
                        }
                        if (nmax < x[n].high) {
                            nmax = x[n].high;
                        }
                        return [x.timestamp, x[n].value];
                    }),
                },
                {
                    name: "high",
                    type: "area",
                    data: all_stats.map(x => [x.timestamp, x[n].high - x[n].low]),
                },
            ];

            // round up nmax to an easily recognizable N-scale (what's that? see below)
            var mult = 1;
            // let's see how the lowest scale value would look like
            var m = Math.trunc((nmax + this.num_hgrid - 1) / this.num_hgrid);
            // leave only the 2 most significant digits of it
            while (m >= 100) {
                m = Math.trunc(m / 10);
                mult *= 10; // keep the multiplier so it'll be easy to restore the rounded-up value
            }
            // so m has at most 2 digits here
            // the "nice" values are 1,2,3,4,5, 10,15,20,25,30, 40,50,60,70,80,90
            // up to five it's OK, we need to tweak only above it
            if (5 < m) {
                // round it up to next multiple of 5
                m = 5 * Math.trunc((m + 4) / 5);
                // the "nice" values are 10,15,20,25,30, 40,50,60,70,80,90
                if ((m != 15) && (m != 25) && ((m % 10) != 0)) {
                    // round it up to next multiple of 10
                    m = 10 * Math.trunc((m + 9) / 10);
                }
            }
            // and restore the trailing zeros
            m *= mult;
            // and restore the top (Nth) grid value
            nmax = m * this.num_hgrid;

            return {
                stats: nstats,
                options: {
                    chart: {
                        id: "vuechart-example",
                        animations: { enabled: false, },
                        toolbar: { show: false, },
                        zoom: { enabled: false, },
                        stacked: true,
                    },
                    stroke: { width: 1, curve: "smooth", },
                    grid: {
                        position: "front",
                        xaxis: { lines: { show: true, }, },
                        yaxis: { lines: { show: true, }, },
                        strokeDashArray: [ 1, 4],
                        borderColor: colors.grey.lighten3,
                    },
                    legend: { show: false, },
                    tooltip: { enabled: false, },
                    colors: [colors.green.base, colors.shades.black, colors.amber.base],
                    xaxis: {
                        type: "datetime",
                        labels: { style: { fontSize: "8", }, offsetY: -8, },
                    },
                    yaxis: {
                        min: 0,
                        max: nmax,
                        tickAmount: this.num_hgrid,
                        labels: { style: { fontSize: "8", }, maxWidth: 20, },
                    },
                },
            };
        },
        details_mode: function () {
            switch (this.detailed_chart_mode) {
                case "week": this.detailed_chart_mode = "month"; break;
                case "month": this.detailed_chart_mode = "week"; break;
            }
            this.details_fetch();
        },
        details_step: function (delta) {
            switch (this.detailed_chart_mode) {
                case "week": 
                    this.detailed_chart_day = this.detailed_chart_day.add_week(delta);
                    break;

                case "month":
                    // FIXME: a month is not 4 week, but now let's just show something
                    this.detailed_chart_day = this.detailed_chart_day.add_week(delta * 4);
                    break;
            }
            this.details_fetch();
        },
        details_fetch: function () {
            const num_days = (this.detailed_chart_mode == "week") ? 7 : 28;
            const raw_data = S4DataSource.get_nutrition_stats({start: this.detailed_chart_day.timestamp - num_days * 24*60*60*1000 , end: this.detailed_chart_day.timestamp});
            this.detailed_chart_data = this.get_nutrition_stats(Object.values(raw_data), this.detailed_chart_nutrient);
        },
        show_details: function(n) {
            this.detailed_chart_nutrient = n;
            this.details_fetch();
            this.showing_detailed_chart = true;
        },
    },
    created: function () {
        const day = new S4Date();
        this.detailed_chart_day = day;
        const num_days = 7;
        this.raw_nutrition_data = S4DataSource.get_nutrition_stats({start: day.timestamp - num_days * 24*60*60*1000 , end: day.timestamp});
        for (let n of this.nutrients) {
            this.nutrition_data[n] = this.get_nutrition_stats(Object.values(this.raw_nutrition_data), n);
        }
    },
}
</script>

<style lang="scss" scoped>
@import "~vuetify/src/styles/main.sass";

    .daily-status {
        height: 100%;
    }

    .nutrient-row {
        flex-grow: 1;
        flex-shrink: 0;
        flex-basis: 0px;
        height: 0px;

        display: flex;
        flex-direction: row;
    }

    .gauge-box {
        display: flex;
        height: 100%;
        margin-top: auto;
        margin-bottom: auto;
        padding-right: 12px;
        padding-top: 6px;
        padding-bottom: 6px;
    }

</style>

// vim: set sw=4 ts=4 et indk= :
