import S4Date from "@/plugins/S4Date"
import ADLER32 from "adler-32"

class PRNG { // by Borland TurboPascal :D
    constructor (seed) {
        this.reset(seed);
    }

    reset(seed) {
        this.value = seed & 0xffff;
    }

    next() {
        // NOTE: bitwise ops are limited to 32-bit operands and 5-bit shift counts,
        // so we're gonna do this in 16-bits...
        // Moreover, numbers above 32 bits are treated as real, so n' = (n * 0x8088405) + 1
        // doesn't work, as the +1 does nothing...
        //
        // so...
        // http://www.retroprogramming.com/2017/07/xorshift-pseudorandom-numbers-in-z80.html
        this.value ^= (this.value << 7) & 0xffff;
        this.value ^= this.value >>> 9;
        this.value ^= (this.value << 8) & 0xffff;
        return this.value;
    }

    real(from = 0, to = 1) {
        return from + (to - from) * this.next() / 0x10000;
    }

    int(from = 0, to = 100) {
        return from + (((to - from) * this.next()) >> 16);
    }

    bool() {
        return !!(this.next() & 1);
    }
}


class S4DataSource {
    static food_providers = [
        { name: "Kaged", logo_url: require("@/assets/logos/kaged.png"), },
        { name: "Labrada", logo_url: require("@/assets/logos/labrada.png"), },
        { name: "Grenade", logo_url: require("@/assets/logos/grenade.png"), },
        { name: "Under 500", logo_url: require("@/assets/logos/under_500.png"), },
        { name: "KetoFix", },
        { name: "German Döner", logo_url: require("@/assets/logos/german_doner.png"), },
        { name: "MuscleTech", logo_url: require("@/assets/logos/muscletech.png"), },
        { name: "Right Bite", logo_url: require("@/assets/logos/right_bite.png"), },
    ];

    static amount_presets = {
        "pcs": [
            { name: "One",          icon: "fas fa-dice-one",            amount: 1, },
            { name: "Few",          icon: "fas fa-dice-two",            amount: 3, },
            { name: "Several",      icon: "fas fa-dice-three",          amount: 7, },
            { name: "Pack",         icon: "fas fa-dice-four",           amount: 15, },
            { name: "Lots",         icon: "fas fa-dice-five",           amount: 35, },
            { name: "Horde",        icon: "fas fa-dice-six",            amount: 75, },
        ],
        "portion": [
            { name: "A bit",        icon: "fas fa-pizza-slice fa-xs",   amount: 0.25, },
            { name: "Half",         icon: "fas fa-pizza-slice fa-sm",   amount: 0.5, },
            { name: "Small",        icon: "fas fa-pizza-slice fa-1x",   amount: 0.75, },
            { name: "One",          icon: "fas fa-cookie fa-xs",        amount: 1, },
            { name: "Big",          icon: "fas fa-cookie fa-sm",        amount: 1.5, },
            { name: "Double",       icon: "fas fa-cookie fa-1x",        amount: 2, },
        ],
        "ml": [
            { name: "Shot",         icon: "fas fa-glass-martini-alt",   amount: 50, },
            { name: "Cf.cup",       icon: "fas fa-glass-martini",       amount: 100, },
            { name: "Cup",          icon: "fas fa-wine-glass-alt",      amount: 150, },
            { name: "Glass",        icon: "fas fa-wine-glass",          amount: 250, },
            { name: "Cf.mug",       icon: "fas fa-glass-whiskey",       amount: 350, },
            { name: "Mug",          icon: "fas fa-coffee",              amount: 500, },
        ],
        "g": [
            { name: "Cf.spoon",     icon: "fas fa-balance-scale-left",  amount: 2, },
            { name: "T.spoon",      icon: "fas fa-balance-scale-left",  amount: 5, },
            { name: "Tbl.spoon",    icon: "fas fa-balance-scale",       amount: 15, },
            { name: "Cup",          icon: "fas fa-balance-scale",       amount: 250, },
            { name: "Bowl",         icon: "fas fa-balance-scale-right", amount: 500, },
            { name: "Other",        icon: "fas fa-balance-scale-right", amount: 1000, },
        ],
    };

    static food_cached = [
        {
            provider: "Under 500",   name: "Acai Bowl",                 unit: "portion",        reference_amount: 1,
            energy: 430, protein:  6, carbs: 74, fat: 13, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
            image_url: require("@/assets/food_cache/under_500-acai_bowl.jpg"),
        },
        {
            provider: "Under 500",   name: "Avocado Smash",             unit: "portion",        reference_amount: 1,
            energy: 430, protein:  6, carbs: 74, fat: 13, water: 10,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
            image_url: require("@/assets/food_cache/under_500-avocado_smash.jpg"),
        },
        {
            provider: "Grenade",   name: "Carb Killa Cookie Dough",     unit: "pcs",            reference_amount: 1,
            energy: 430, protein:  6, carbs: 74, fat: 13, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
            image_url: require("@/assets/food_cache/grenade-carb_killa_cookie_dough.jpg"),
        },
        {
            provider: "Labrada",   name: "Lean Body Cafe Mocha",        unit: "pcs",            reference_amount: 1,
            energy: 430, protein:  6, carbs: 74, fat: 13, water: 200,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
            image_url: require("@/assets/food_cache/labrada-lean_body_cafe_mocha.jpg"),
        },
        {
            provider: "Bosporus",  name: "Bos Burger",                  unit: "pcs",            reference_amount: 1,
            energy: 430, protein:  6, carbs: 74, fat: 13, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
            image_url: require("@/assets/food_cache/bosporus-bos_burger.jpg"),
        },
        {
            provider: "Rétisas",  name: "Mátrai borzas",                unit: "portion",        reference_amount: 1,
            energy: 430, protein:  6, carbs: 74, fat: 13, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
            image_url: require("@/assets/food_cache/retisas-matrai_borzas.jpg"),
        },
    ];

    static food_list = [
        {
            provider: "Nando's",      name: "Espetada Carnival",        unit: "portion",        reference_amount: 1,
            energy: 123, protein: 34, carbs: 12, fat: 18, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
        },
        {
            provider: "Oxylent",      name: "Multivitamin Drink",       unit: "ml",             reference_amount: 100,
            energy:   0, protein:  0, carbs:  0, fat:  0, water: 100,
            vit_A: 400, vit_B1: 1.5, vit_B2: 1.7, vit_B3: 20, vit_B5: 10, vit_B6: 10, vit_B7: 0, vit_B9: 100, vit_B12: 50, vit_C: 600, vit_D: 25, vit_E: 15, vit_K: 0,
        },
        {
            provider: "<Generic>",    name: "Grapefruit",               unit: "pcs",            reference_amount: 1,
            energy: 100, protein: 1.8, carbs: 24.5, fat:  0.3, water: 250,
            vit_A: 800, vit_B1: 0.1, vit_B2: 0.1, vit_B3: 0.5, vit_B5: 0.6, vit_B6: 0.1, vit_B7: 0, vit_B9: 30, vit_B12: 50, vit_C: 71.8, vit_D: 0, vit_E: 0.3, vit_K: 0,
        },
        {
            provider: "<Generic>",    name: "Peanuts",                  unit: "g",              reference_amount: 100,
            energy: 100, protein: 1.8, carbs: 24.5, fat:  0.3, water: 0,
            vit_A: 800, vit_B1: 0.1, vit_B2: 0.1, vit_B3: 0.5, vit_B5: 0.6, vit_B6: 0.1, vit_B7: 0, vit_B9: 30, vit_B12: 50, vit_C: 71.8, vit_D: 0, vit_E: 0.3, vit_K: 0,
        },
        {
            provider: "<Generic>",    name: "Milk",                     unit: "ml",             reference_amount: 100,
            energy: 100, protein: 1.8, carbs: 24.5, fat:  0.3, water: 100,
            vit_A: 800, vit_B1: 0.1, vit_B2: 0.1, vit_B3: 0.5, vit_B5: 0.6, vit_B6: 0.1, vit_B7: 0, vit_B9: 30, vit_B12: 50, vit_C: 71.8, vit_D: 0, vit_E: 0.3, vit_K: 0,
        },
        {
            provider: "Nando's",      name: "Cataplana",                unit: "portion",        reference_amount: 1,
            energy: 521, protein: 73, carbs: 42, fat: 68, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
        },
        {
            provider: "Nando's",      name: "Chicken Burger",           unit: "pcs",            reference_amount: 1,
            energy: 732, protein: 10, carbs: 82, fat: 26, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
        },
        {
            provider: "German Döner", name: "Gym Box",                  unit: "portion",        reference_amount: 1,
            energy: 212, protein: 43, carbs: 18, fat: 25, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
        },
        {
            provider: "German Döner", name: "Kebab Sandwich",           unit: "pcs",            reference_amount: 1,
            energy: 824, protein: 12, carbs: 68, fat: 73, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
        },
        {
            provider: "German Döner", name: "Kebab Meal",               unit: "portion",        reference_amount: 1,
            energy: 197, protein: 42, carbs: 26, fat: 68, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
        },
        {
            provider: "Under 500",    name: "Steak & Eggs",             unit: "portion",        reference_amount: 1,
            energy: 322, protein: 82, carbs: 25, fat: 23, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
        },
        {
            provider: "Under 500",    name: "Avocado Smash",            unit: "portion",        reference_amount: 1,
            energy: 541, protein: 18, carbs: 73, fat: 62, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
        },
        {
            provider: "Under 500",    name: "Acai Bowl",                unit: "portion",        reference_amount: 1,
            energy: 857, protein: 68, carbs: 68, fat: 72, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
        },
        {
            provider: "KetoFix",      name: "Pepperoni Pizza",          unit: "pcs",            reference_amount: 1,
            energy: 267, protein: 26, carbs: 23, fat: 90, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
        },
        {
            provider: "Right Bite",   name: "Swiss Burger",             unit: "pcs",            reference_amount: 1,
            energy: 727, protein: 25, carbs: 62, fat: 34, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
        },
        {
            provider: "Right Bite",   name: "Chili Beef w/ Zoodles",    unit: "portion",        reference_amount: 1,
            energy: 916, protein: 73, carbs: 72, fat: 73, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
        },
        {
            provider: "Right Bite",   name: "Salmon Bowl",              unit: "portion",        reference_amount: 1,
            energy: 802, protein: 68, carbs: 90, fat: 10, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
        },
        {
            provider: "Bowl Is Life", name: "Shoyu",                    unit: "portion",        reference_amount: 1,
            energy: 459, protein: 23, carbs: 34, fat: 43, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
        },
        {
            provider: "Go! Healthy",  name: "Grilled Tenderloin",       unit: "portion",        reference_amount: 1,
            energy: 272, protein: 62, carbs: 73, fat: 12, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
        },
        {
            provider: "Fryd",         name: "8 pcs Chicken Tenders",    unit: "portion",        reference_amount: 1,
            energy: 372, protein: 72, carbs: 10, fat: 42, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
        },
        {
            provider: "Krave",        name: "Grilled Chicken Breast",   unit: "portion",        reference_amount: 1,
            energy: 443, protein: 90, carbs: 43, fat: 82, water: 0,
            vit_A: 0, vit_B1: 0, vit_B2: 0, vit_B3: 0, vit_B5: 0, vit_B6: 0, vit_B7: 0, vit_B9: 0, vit_B12: 0, vit_C: 0, vit_D: 0, vit_E: 0, vit_K: 0,
        },
    ];

    static food_tags_available = [ "Bakery", "Bowl", "Burger", "Dairy", "Dish", "Drink", "Fruit", "Meat", "Nutrient", "Pizza", "ProteinBar", "Salad", "Sandwich", "Soup", "Supplement", "Sweets", "Wrap" ];

    static workout_types = [ "Invigoration", "FatBurn", "Stretching", "Yoga", "Cardio" ];

    static titles = [ "Mr.", "Mrs.", "Ms.", "Dr.", "Prof.", "Rev.", "Sir", "Lady", "HH.", "Gen.", "Col.", "Maj.", "Cpt." ]; // just to name a few :)

    static languages = [
        {
            id: "eng",
            name: "English",
        },
        {
            id: "ara",
            name: "Arabic",
        },
        {
            id: "hin",
            name: "Hindi",
        },
        {
            id: "urd",
            name: "Urdu",
        },
        {
            id: "fil",
            name: "Filipino",
        },
        {
            id: "mly",
            name: "Malay",
        },
    ];

    static get_events(iv) {
        // iv.start and iv.end are timestamps, the date is represented as the date part of that timestamp assumed the timezone UTC
        // NO local timezone support, it makes no sense for us

        console.log("get_events " + iv.start + " .. " + iv.end);

        // drop the time-within-the day part
        const start = (0|(iv.start / S4Date.ONE_DAY)) * S4Date.ONE_DAY;
        const end = (0|(iv.end / S4Date.ONE_DAY)) * S4Date.ONE_DAY;

        const days = {};


        for (let day = start; day <= end; day += S4Date.ONE_DAY) {
            const dd = new Date(day);
            const day_str = S4Date.date2str(dd);
            var prng = new PRNG(day);

            switch (dd.getDay()) {
                case 0: { // Sunday
                    console.log("Workout day: " + day_str);
                    days[day_str] = {
                        title: "Workout",
                        class: "workout-day",
                        events: [
                            {
                                title:  "Invigoration",
                                class: "workout",
                                start: 15.5 * 60,
                                end: 15.5 * 60 + 30,
                                trainer: "Arun",
                            },
                            {
                                title: "Stretching",
                                class: "workout",
                                start: 16.0 * 60,
                                end: 16.0 * 60 + 30,
                                trainer: "Arun",
                            },
                        ],
                    };
                    break;
                }
                case 6: { // Saturday
                    console.log("Cheat day: " + day_str);
                    days[day_str] = {
                        title: "Cheat",
                        class: "cheat-day",
                        events: [],
                    };
                    break;
                }
                default: {
                    if (prng.int() >= 10) {
                        console.log("Normal day: " + day_str);
                        days[day_str] = {
                            title: "Normal",
                            class: "normal-day",
                            events: [],
                        };
                    }
                    else {
                        console.log("Workout day: " + day_str);
                        const workout_type = prng.int(0, this.workout_types.length);
                        const workout_start = prng.int(8*2, 18*2) / 2;
                        days[day_str] = {
                            title: "Workout",
                            class: "workout-day",
                            events: [
                                {
                                    title: this.workout_types[workout_type],
                                    class: "workout",
                                    start: workout_start * 60,
                                    end: workout_start * 60 + 30,
                                    trainer: "Shaphic",
                                },
                            ],
                        };
                    }
                    break;
                }
            }
        }
        return days;
    }

    static get_nutrition_stats(iv) {
        // iv.start and iv.end are timestamps, the date is represented as the date part of that timestamp assumed the timezone UTC
        // NO local timezone support, it makes no sense for us

        console.log("get_nutrition_status " + iv.start + " .. " + iv.end);

        // drop the time-within-the day part
        const start = (0|(iv.start / S4Date.ONE_DAY)) * S4Date.ONE_DAY;
        const end = (0|(iv.end / S4Date.ONE_DAY)) * S4Date.ONE_DAY;

        const days = {};

        for (let day = start; day <= end; day += S4Date.ONE_DAY) {
            const dd = new Date(day);
            const day_str = S4Date.date2str(dd);
            var prng = new PRNG(day);

            var stats = {
                timestamp: dd.getTime(),
                energy: {
                    low: 1200 + Math.round(prng.bool() ? 400 : 0),
                    high: 2400 + Math.round(prng.bool() ? 400 : 0),
                },
                protein: {
                    low: 80 + Math.round(prng.bool() ? 20 : 0),
                    high: 120 + Math.round(prng.bool() ? 20 : 0),
                },
                carbs: {
                    low: 0 + Math.round(prng.bool() ? 20 : 0),
                    high: 80 + Math.round(prng.bool() ? 20 : 0),
                },
                fat: {
                    low: 20 + Math.round(prng.bool() ? 20 : 0),
                    high: 60 + Math.round(prng.bool() ? 20 : 0),
                },
                water: {
                    low: 1800 + Math.round(prng.bool() ? 400 : 0),
                    high: 2600 + Math.round(prng.bool() ? 200 : 0),
                },
            };
        
            for (let nutrient of ["energy", "protein", "carbs", "fat", "water"]) {
                stats[nutrient].value = Math.round(prng.real(0.2, 1.2) * stats[nutrient].high);
            }
            days[day_str] = stats;
        }
        return days;
    }

    static trainers = [
        { name: "Arun", },
        { name: "Shaphic", },
        { name: "Salah", },
    ];

    static get_trainers_available(iv, trainers) {
        if (!trainers || (trainers.length == 0)) {
            trainers = this.trainers.map(t => t.name);
        }
        console.log("get_trainers_available " + iv.start + " .. " + iv.end);

        // drop the time-within-the day part
        const start = (0|(iv.start / S4Date.ONE_DAY)) * S4Date.ONE_DAY;
        const end = (0|(iv.end / S4Date.ONE_DAY)) * S4Date.ONE_DAY;

        const days = {};

        for (let day = start; day <= end; day += S4Date.ONE_DAY) {
            const dd = new Date(day);
            const day_str = S4Date.date2str(dd);
            const tmask = trainers.map(t => {
                const m = ADLER32.str(t + day_str);
                return { name: t, mask: m & (m << 1) };
            });
            let hours = {};
            for (let hour = 8; hour <= 21.5; hour += 0.5) {
                const bm = 1 << ((hour - 8) * 2);
                const tt = tmask.filter(t => !!(t.mask & bm)).map(t => t.name);
                if (tt.length) {
                    hours[hour] = tmask.filter(t => !!(t.mask & bm)).map(t => t.name);
                }
            }
            // if (!hours.isEmpty()) days[day_str] = hours;
            for (let x in hours) { // eslint-disable-line no-unused-vars
                days[day_str] = hours;
                break;
            }
        }
        return days;
    }

    static install (Vue, options) { // eslint-disable-line no-unused-vars
    }

    constructor () {
    }

}

export default S4DataSource;
// vim: set sw=4 ts=4 indk= et:
