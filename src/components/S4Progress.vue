<!-- Saga4 Calendar component -->
<template>
    <div class="s4-progress">
        <!--div v-if="!!title" class="text-h5">{{ title }}</div-->

        <svg class="s4-progress-background" viewBox="0 0 100 100" width="100%" height="100%" xmlns="http://www.w3.org/2000/svg">
            <!-- the background -->
            <circle :cx="cx" :cy="cy" :r="radius + stroke_width" :fill="background" :stroke="colorBorder"/>
        </svg>

        <div class="s4-progress-upper">
            <slot name="upper">
                <span>{{ title }}</span>
            </slot>
        </div>
        <div class="s4-progress-lower">
            <slot name="lower">
                <span>{{ value }}</span>
                <span v-if="!!unit"> {{ unit }}</span>
            </slot>
        </div>

        <svg class="s4-progress-gauge" viewBox="0 0 100 100" width="100%" height="100%" xmlns="http://www.w3.org/2000/svg">
            <!-- the part from _min to _low -->
            <template v-if="_low <= _value">
                <circle v-if="c_low > 0"
                    :cx="cx" :cy="cy" :r="radius" fill="transparent" :stroke-width="stroke_width" :transform="transform" :stroke="colorLowDone"
                    :stroke-dashoffset="0" :stroke-dasharray="(c_low - 0) + ',' + infinity"
                />
            </template>
            <template v-else>
                <circle v_if="c_value > 0"
                    :cx="cx" :cy="cy" :r="radius" fill="transparent" :stroke-width="stroke_width" :transform="transform" :stroke="colorLowDone"
                    :stroke-dashoffset="0" :stroke-dasharray="(c_value - 0) + ',' + infinity"
                />
                <circle v_if="c_low > c_value"
                    :cx="cx" :cy="cy" :r="radius" fill="transparent" :stroke-width="stroke_width" :transform="transform" :stroke="colorLowFree"
                    :stroke-dashoffset="-c_value" :stroke-dasharray="(c_low - c_value) + ',' + infinity"
                />
            </template>

            <!-- the part from _low to _high -->
            <template v-if="_value <= _low">
                <circle v-if="c_high > c_low"
                    :cx="cx" :cy="cy" :r="radius" fill="transparent" :stroke-width="stroke_width" :transform="transform" :stroke="colorMidFree"
                    :stroke-dashoffset="-c_low" :stroke-dasharray="(c_high - c_low) + ',' + infinity"
                />
            </template>
            <template v-else-if="_value <= _high">
                <circle v-if="c_value > c_low"
                    :cx="cx" :cy="cy" :r="radius" fill="transparent" :stroke-width="stroke_width" :transform="transform" :stroke="colorMidDone"
                    :stroke-dashoffset="-c_low" :stroke-dasharray="(c_value - c_low) + ',' + infinity"
                />
                <circle v-if="c_high > c_value"
                    :cx="cx" :cy="cy" :r="radius" fill="transparent" :stroke-width="stroke_width" :transform="transform" :stroke="colorMidFree"
                    :stroke-dashoffset="-c_value" :stroke-dasharray="(c_high - c_value) + ',' + infinity"
                />
            </template>
            <template v-else>
                <circle v-if="c_high > c_low"
                    :cx="cx" :cy="cy" :r="radius" fill="transparent" :stroke-width="stroke_width" :transform="transform" :stroke="colorMidDone"
                    :stroke-dashoffset="-c_low" :stroke-dasharray="(c_high - c_low) + ',' + infinity"
                />
            </template>

            <!-- the part from _high to _max -->
            <template v-if="_value <= _high">
                <circle v-if="c_max > c_high"
                    :cx="cx" :cy="cy" :r="radius" fill="transparent" :stroke-width="stroke_width" :transform="transform" :stroke="colorHighFree"
                    :stroke-dashoffset="-c_high" :stroke-dasharray="(c_max - c_high) + ',' + infinity"
                />
            </template>
            <template v-else>
                <circle v-if="c_value > c_high"
                    :cx="cx" :cy="cy" :r="radius" fill="transparent" :stroke-width="stroke_width" :transform="transform" :stroke="colorHighDone"
                    :stroke-dashoffset="-c_high" :stroke-dasharray="(c_value - c_high) + ',' + infinity"
                />
                <circle v-if="c_max > c_value"
                    :cx="cx" :cy="cy" :r="radius" fill="transparent" :stroke-width="stroke_width" :transform="transform" :stroke="colorHighFree"
                    :stroke-dashoffset="-c_value" :stroke-dasharray="(c_max - c_value) + ',' + infinity"
                />
            </template>

            <!-- the inner indicator -->
            <circle
                :cx="cx" :cy="cy" :r="radius" fill="transparent" :stroke-width="stroke_inner" :transform="transform" :stroke="colorFree"
                :stroke-dashoffset="-c_value" :stroke-dasharray="(c_max - c_value) + ',' + infinity"
            />

            <!-- the arm -->
            <line :x1="cx" :y1="cy" :x2="arm_x" :y2="arm_y" :stroke="colorArm"/>
            <circle :cx="cx" :cy="cy" :r="stroke_width / 2" :fill="colorArm"/>
        </svg>

    </div>
</template>

<script>

import colors from "vuetify/lib/util/colors";

export default {
    name: "S4Progress",
    props: {
        title: String,
        unit: String,
        min: Number,
        low: Number,
        high: Number,
        max: Number,
        value: Number,
        data: Array,    // either specify low, high and value, or data: [ {low: ..., high: ..., value: ...}, {...}, ...]
        background: {
            type: String,
            default: colors.grey.lighten3,
        },
        colorLowDone: {
            type: String,
            default: colors.green.base,
        },
        colorLowFree: {
            type: String,
            default: colors.green.lighten3,
        },
        colorMidDone: {
            type: String,
            default: colors.amber.base,
        },
        colorMidFree: {
            type: String,
            default: colors.amber.lighten3,
        },
        colorHighDone: {
            type: String,
            default: colors.red.base,
        },
        colorHighFree: {
            type: String,
            default: colors.red.lighten3,
        },
        colorFree: {
            type: String,
            default: colors.shades.white,
        },
        colorArm: {
            type: String,
            default: colors.shades.black,
        },
        colorBorder: {
            type: String,
            default: colors.grey.lighten2,
        },
        gap: {
            type: Number,
            default: 90,
        },
    },
    data() {
        return {
        };
    },
    computed: {
        // geometry
        infinity: function () {
            console.log("Progress; title=" + this.title + ", _min=" + this._min +", _low=" + this._low + ", _value=" + this._value + ", _high=" + this._high + ", _max=" + this._max);
            console.log("Progress; title=" + this.title + ", c_low=" + this.c_low + ", c_value=" + this.c_value + ", c_high=" + this.c_high + ", circumference=" + this.circumference);
            return 999999;
        },
        radius: function () {
            return 40;
        },
        rotate: function () {
            return 90 + (this.gap / 2);
        },
        circumference: function () {
            return (this.radius * 2 * Math.PI) * (360 - this.gap) / 360;
        },
        c_low: function () {
            return Math.round(this.circumference * (this._low - this._min) / (this._max - this._min));
        },
        c_value: function () {
            return Math.round(this.circumference * (this._value - this._min) / (this._max - this._min));
        },
        c_high: function () {
            return Math.round(this.circumference * (this._high - this._min) / (this._max - this._min));
        },
        c_max: function () {
            return Math.round(this.circumference);
        },
        stroke_width: function () {
            return 10;
        },
        stroke_inner: function () {
            return 4;
        },
        cx: function () {
            return 50;
        },
        cy: function () {
            return 50;
        },
        transform: function () {
            return "rotate(" + this.rotate + " " + this.cx + " " + this.cy + ")";
        },
        arm_x: function () {
            return this.cx + (this.radius + this.stroke_width) * Math.cos((Math.PI * this.rotate / 180) + (this.c_value / this.radius));
        },
        arm_y: function () {
            return this.cx + (this.radius + this.stroke_width) * Math.sin((Math.PI * this.rotate / 180) + (this.c_value / this.radius));
        },
        // low, high, value may come as scalar props or as attributes of the last element of the data array
        _min: function () {
            if (!this.data || (this.data.length == 0)) {
                return this.min || 0;
            }
            return this.data[this.data.length - 1].min || this.min || 0
        },
        _low: function () {
            if (!this.data || (this.data.length == 0)) {
                return this.low || this._min;
            }
            return this.data[this.data.length - 1].low || this.low || this._min;
        },
        _max: function () {
            if (!this.data || (this.data.length == 0)) {
                if (typeof this.max !== "undefined") {
                    return this.max;
                }
                if (typeof this.high !== "undefined") {
                    return this.high * 1.05;
                }
                return 105;
            }
            if ("max" in this.data[this.data.length - 1]) {
                return this.data[this.data.length - 1].max;
            }
            if ("high" in this.data[this.data.length - 1]) {
                return this.data[this.data.length - 1].high * 1.05;
            }
            return 105;
        },
        _high: function () {
            if (!this.data || (this.data.length == 0)) {
                return this.high || this.max || 100; 
            }
            return this.data[this.data.length - 1].high || this.high || this.data[this.data.length - 1].max || this.max || 100;
        },
        _value: function () {
            var v;
            if (!this.data || (this.data.length == 0)) {
                v = this.value;
            }
            else {
                v = this.data[this.data.length - 1].value || this.value || 42;
            }
            return Math.min(this._max, Math.max(this._min, v));
        },

    },
}
</script>

<style lang="scss" scoped>
@import "~vuetify/src/styles/main.sass";

    .s4-progress {
        position: relative;
        flex-grow: 1;
    }

    .s4-progress-background, .s4-progress-gauge {
        position: absolute;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
    }

    .s4-progress-background {
        z-index: 0;
     }
     
    .s4-progress-gauge {
        z-index: 2;
    }

    .s4-progress-upper, .s4-progress-lower {
        position: absolute;
        width: 100%;
        z-index: 1;
        display: flex;
        align-items: center;
        justify-content: center;
    }

    .s4-progress-upper {
        top: 25%;
        height: 15%;
    }

    .s4-progress-lower {
        top: 60%;
        height: 25%;
    }

</style>

// vim: set sw=4 ts=4 et indk= :
