<template>
    <div class="nutrition-tracker">
        <!--
        Chooser for the most frequent / most recent foods
        Picture + name + perhaps short summary
        -->
        <v-sheet class="mx-auto" max-width="700">
            <v-slide-group center-active>

                <v-slide-item :key="'foodcard-' + food.provider + '-' + food.name" v-for="food in S4DataSource.food_cached">
                    <v-card class="food-card" @click="select_food(food)">
                        <v-img class="food-picker" :src="food.image_url">
                            <v-card-title>{{ food.name }}</v-card-title>
                        </v-img>
                        <v-row dense>
                            <v-col cols="8">
                                <span class="white--text font-weight-black font-italic pa-0">{{ food.provider }}</span>
                            </v-col>
                            <v-col cols="4">
                                <span>Cal:{{ food.energy }}</span>
                            </v-col>
                        </v-row>
                        <v-row dense>
                            <v-col cols="3">
                                <span>P:{{ food.protein }}</span>
                            </v-col>
                            <v-col cols="3">
                                <span>C:{{ food.carbs }}</span>
                            </v-col>
                            <v-col cols="3">
                                <span>F:{{ food.fat }}</span>
                            </v-col>
                            <v-col cols="3">
                                <span>W:{{ food.water }}</span>
                            </v-col>
                        </v-row>
                    </v-card>
                </v-slide-item>

            </v-slide-group>
        </v-sheet>

        <!--
        Select food from the database
        Narrowing options are by provider and by tag and perhaps by freetext search
        -->
        <v-sheet class="mx-auto" max-width="700">

            <v-row dense id="food-filter-tools">

                <v-col cols="4">
                    <v-bottom-sheet v-model="choosing_food_provider" scrollable>
                        <template v-slot:activator="{ on, attrs }">
                            <v-btn v-bind="attrs" v-on="on" color="primary" block><v-icon small>fas fa-utensils</v-icon>/<v-icon small>fas fa-industry</v-icon></v-btn>
                        </template>
                        <v-card>
                            <v-card-title>Choose tags</v-card-title>
                            <v-card-text style="height: 60vh;">
                                <v-list>
                                    <v-subheader>Choose provider</v-subheader>
                                    <v-list-item-group v-model="food_provider" color="primary">
                                        <v-list-item v-for="prov in S4DataSource.food_providers" :key="prov.name" @click="choosing_food_provider = false">
                                            <v-list-item-icon>
                                                <v-avatar size="32px" tile>
                                                    <img v-if="!!prov.logo_url" :src="prov.logo_url">
                                                </v-avatar>
                                            </v-list-item-icon>
                                            <v-list-item-content>{{ prov.name }}</v-list-item-content>
                                        </v-list-item>
                                    </v-list-item-group>
                                </v-list>
                            </v-card-text>
                        </v-card>
                    </v-bottom-sheet>
                </v-col>

                <v-col cols="4">
                    <v-bottom-sheet v-model="choosing_food_tags" scrollable>
                        <template v-slot:activator="{ on, attrs }">
                            <v-btn v-bind="attrs" v-on="on" color="primary" block><v-icon small>fas fa-filter</v-icon></v-btn>
                        </template>
                        <v-card>
                            <v-card-title>Choose tags</v-card-title>
                            <v-card-text style="height: 60vh;">
                                <v-list>
                                    <v-list-item-group v-model="food_tag_indices" color="primary" multiple>
                                        <v-list-item v-for="tag in S4DataSource.food_tags_available" :key="tag" @click="choosing_food_tags = false">
                                            <template v-slot:default="{ active }">
                                                <v-list-item-action>
                                                    <v-checkbox :input-value="active" color="primary"></v-checkbox>
                                                </v-list-item-action>
                                                <v-list-item-content>{{ tag }}</v-list-item-content>
                                            </template>
                                        </v-list-item>
                                    </v-list-item-group>
                                </v-list>
                            </v-card-text>
                        </v-card>
                    </v-bottom-sheet>
                </v-col>

                <v-col cols="4">
                    <v-dialog v-model="entering_food_freetext" max-width="80vw">
                        <template v-slot:activator="{ on, attrs }">
                            <v-btn v-bind="attrs" v-on="on" color="primary" block><v-icon small>fas fa-search</v-icon></v-btn>
                        </template>
                        <v-card>
                            <v-card-actions>
                                <v-text-field label="Food name" placeholder="<partial name>" append-icon="fas fa-search" v-model="food_freetext_input" @click:append="food_freetext_search"></v-text-field>
                            </v-card-actions>
                        </v-card>
                    </v-dialog>
                </v-col>

            </v-row>

            <div id="active-tags">
                <v-chip v-for="tag_ii_plus_one in food_tag_indices.length" :key="'tag-' + tag_ii_plus_one" small class="pa-2" close color="primary" @click:close="food_tag_indices.splice(tag_ii_plus_one - 1, 1)">
                    {{ S4DataSource.food_tags_available[food_tag_indices[tag_ii_plus_one - 1]] }}
                </v-chip>
            </div>

			<v-list-item id="food-list-header">
				<v-row dense>
					<v-col cols="4" class="text-left">Name</v-col>
					<v-col cols="3">Provider</v-col>
					<v-col cols="1">E</v-col>
					<v-col cols="1">P</v-col>
					<v-col cols="1">C</v-col>
					<v-col cols="1">F</v-col>
					<v-col cols="1">W</v-col>
				</v-row>
			</v-list-item>
            <!-- header=48px filter-buttons=44px tags=24px titlebar=48px bottomnav=56px -->
			<v-virtual-scroll id="food-list" :items="S4DataSource.food_list" item-height="32">
				<template v-slot="x">
					<!-- x.index == 42, x.item == { ... } -->
					<v-list-item @click="select_food(x.item)">
						<v-row dense>
							<v-col cols="4" class="text-left">{{ x.item.name }}</v-col>
							<v-col cols="3">{{ x.item.provider }}</v-col>
							<v-col cols="1">{{ x.item.energy }}</v-col>
							<v-col cols="1">{{ x.item.protein }}</v-col>
							<v-col cols="1">{{ x.item.carbs }}</v-col>
							<v-col cols="1">{{ x.item.fat }}</v-col>
							<v-col cols="1">{{ x.item.water }}</v-col>
						</v-row>
					</v-list-item>
				</template>
			</v-virtual-scroll>                        

        </v-sheet>

        <!--
        Alert for mocking the submit step
        -->
        <v-dialog v-model="show_food_booked" max-width="60vw">
            <v-card>
                <v-card-title>
                    Booking complete
                </v-card-title>
                <v-card-actions>
                    <v-row justify="center">
                        <v-btn color="success" @click="show_food_booked = false">OK</v-btn>
                    </v-row>
                </v-card-actions>
            </v-card>
        </v-dialog>

        <v-dialog v-model="selecting_food_amount" max-width="90vw" modal>
            <v-card v-if="!!selected_food">
                <v-card-title class="d-block text-h6">Choosing amounts for</v-card-title>
                <v-card-title class="d-block text-h6" align="center" justify="center">{{ selected_food.name }}</v-card-title>

                <v-card-title class="d-block text-h5">
                    {{ food_amount }} {{ selected_food.unit }}
                </v-card-title>

                <v-btn-toggle dense mandatory v-model="preset_idx">
                    <v-btn v-for="idx_plus_one in S4DataSource.amount_presets[selected_food.unit].length" :key="'amount-' + idx_plus_one" :value="idx_plus_one - 1" color="warning" small>
                        <div>
                            <v-icon small dense>{{ S4DataSource.amount_presets[selected_food.unit][idx_plus_one - 1].icon }}</v-icon>
                            <!-- {{ S4DataSource.amount_presets[selected_food.unit][idx_plus_one - 1].name }} -->
                        </div>
                    </v-btn>
                </v-btn-toggle>

                <v-row dense>
                    <v-col cols="9">
                        <v-btn color="error" @click="book_selected_food" rounded block><v-icon x-small>fas fa-check</v-icon></v-btn>
                    </v-col>
                    <v-col cols="3">
                        <v-btn color="accent" @click="show_preset_amount_changer" small fab><v-icon x-small>fas fa-edit</v-icon></v-btn>
                    </v-col>
                </v-row>

                <!--
                Details of the selected food
                -->
                <v-divider></v-divider>
                <v-sheet class="mx-auto grey lighten-5 text-caption white--text" max-width="700">
                    <v-row dense v-if="contains_macro">
                        <v-col cols="3" class="color-energy">Energy: {{ selected_contents("energy") }}</v-col>
                        <v-col cols="3" class="color-orange">Protein: {{ selected_contents("protein") }}</v-col>
                        <v-col cols="3" class="color-carbs">Carbs: {{ selected_contents("carbs") }}</v-col>
                        <v-col cols="3" class="color-fat">Fat: {{ selected_contents("fat") }}</v-col>
                    </v-row>
                    <v-row dense>
                        <v-col cols="2" v-if="contains_vitamin" class="color-vitamin">A: {{ selected_contents("vit_A") }}</v-col><v-col cols="2" v-else></v-col>
                        <v-col cols="2" v-if="contains_vitamin" class="color-vitamin">B<sub>1</sub>: {{ selected_contents("vit_B1") }}</v-col><v-col cols="2" v-else></v-col>
                        <v-col cols="2" v-if="contains_vitamin" class="color-vitamin">B<sub>2</sub>: {{ selected_contents("vit_B2") }}</v-col><v-col cols="2" v-else></v-col>
                        <v-col cols="3" v-if="contains_macro" class="color-carbs-sub">Sugar: 5.6</v-col><v-col cols="2" v-else></v-col>
                        <v-col cols="3" v-if="contains_macro" class="color-fat-sub">Sat'd: 23</v-col><v-col cols="2" v-else></v-col>
                    </v-row>
                    <v-row dense>
                        <v-col cols="2" v-if="contains_vitamin" class="color-vitamin">B<sub>3</sub>: {{ selected_contents("vit_B3") }}</v-col><v-col cols="2" v-else></v-col>
                        <v-col cols="2" v-if="contains_vitamin" class="color-vitamin">B<sub>5</sub>: {{ selected_contents("vit_B5") }}</v-col><v-col cols="2" v-else></v-col>
                        <v-col cols="2" v-if="contains_vitamin" class="color-vitamin">B<sub>6</sub>: {{ selected_contents("vit_B6") }}</v-col><v-col cols="2" v-else></v-col>
                        <v-col cols="3" v-if="contains_macro" class="color-carbs-sub">Polyol: 5.6</v-col><v-col cols="2" v-else></v-col>
                        <v-col cols="3" v-if="contains_macro" class="color-fat-sub">Mono: 23</v-col><v-col cols="2" v-else></v-col>
                    </v-row>
                    <v-row dense>
                        <v-col cols="2" v-if="contains_vitamin" class="color-vitamin">B<sub>7</sub>: {{ selected_contents("vit_B7") }}</v-col><v-col cols="2" v-else></v-col>
                        <v-col cols="2" v-if="contains_vitamin" class="color-vitamin">B<sub>9</sub>: {{ selected_contents("vit_B9") }}</v-col><v-col cols="2" v-else></v-col>
                        <v-col cols="2" v-if="contains_vitamin" class="color-vitamin">B<sub>12</sub>: {{ selected_contents("vit_B12") }}</v-col><v-col cols="2" v-else></v-col>
                        <v-col cols="3" v-if="contains_macro" class="color-carbs-sub">Fiber: 5.6</v-col><v-col cols="2" v-else></v-col>
                        <v-col cols="3" v-if="contains_macro" class="color-fat-sub">Poly: 23</v-col><v-col cols="2" v-else></v-col>
                    </v-row>
                    <v-row dense>
                        <v-col cols="2" v-if="contains_vitamin" class="color-vitamin">C: {{ selected_contents("vit_C") }}</v-col><v-col cols="2" v-else></v-col>
                        <v-col cols="2" v-if="contains_vitamin" class="color-vitamin">D: {{ selected_contents("vit_D") }}</v-col><v-col cols="2" v-else></v-col>
                        <v-col cols="2" v-if="contains_vitamin" class="color-vitamin">E: {{ selected_contents("vit_E") }}</v-col><v-col cols="2" v-else></v-col>
                        <v-col cols="3" v-if="contains_macro" class="color-carbs-sub">Other: 5.6</v-col><v-col cols="2" v-else></v-col>
                        <v-col cols="3" v-if="contains_macro" class="color-fat-sub">Trans: 23</v-col><v-col cols="2" v-else></v-col>
                    </v-row>
                    <v-row dense>
                        <v-col cols="2" v-if="contains_vitamin" class="color-vitamin">K: {{ selected_contents("vit_K") }}</v-col><v-col cols="2" v-else></v-col>
                        <v-col cols="4" v-if="contains_vitamin" class="color-vitamin"></v-col><v-col cols="4" v-else></v-col>
                        <v-col cols="6" v-if="contains_macro" class="color-water">Water: {{ selected_contents("water") }}</v-col><v-col cols="6" v-else></v-col>
                    </v-row>
                    <!--v-row dense>
                        <v-col cols="2" class="color-mineral">Cr: 300</v-col>
                        <v-col cols="2" class="color-mineral">Co: 13.4</v-col>
                        <v-col cols="2" class="color-mineral">Cu: 11.7</v-col>
                        <v-col cols="2" class="color-mineral">I: 300</v-col>
                        <v-col cols="2" class="color-mineral">Fe: 13.4</v-col>
                        <v-col cols="2" class="color-mineral">Mn: 11.7</v-col>
                    </v-row>
                    <v-row dense>
                        <v-col cols="2" class="color-mineral">Mo: 300</v-col>
                        <v-col cols="2" class="color-mineral">Se: 13.4</v-col>
                        <v-col cols="2" class="color-mineral">Zn: 11.7</v-col>
                        <v-col cols="2" class="color-mineral">Mg: 300</v-col>
                        <v-col cols="2" class="color-mineral">Ca: 13.4</v-col>
                        <v-col cols="2" class="color-mineral">K: 11.7</v-col>
                    </v-row>
                    <v-row dense>
                        <v-col cols="2" class="color-mineral">Na: 300</v-col>
                        <v-col cols="2" class="color-mineral">P: 13.4</v-col>
                        <v-col cols="2" class="color-mineral"></v-col>
                        <v-col cols="2" class="color-mineral"></v-col>
                        <v-col cols="2" class="color-mineral"></v-col>
                        <v-col cols="2" class="color-mineral"></v-col>
                    </v-row-->
                </v-sheet>
            </v-card>
        </v-dialog>

        <!--
        Alert for mocking the submit step
        -->
        <v-dialog v-model="entering_preset_amount" max-width="60vw">
            <v-card>
                <v-card-title>
                    Enter amount
                </v-card-title>
                <v-card-text>
                    <v-icon x-large dense>{{ proposed_preset_icon }}</v-icon>
                    <br/>
                    <v-text-field v-model="proposed_preset_amount" label="Amount" placeholder="<new amount>"></v-text-field>
                    <br/>
                    <v-btn color="error" @click="change_preset_amount" small fab><v-icon x-small>fas fa-check</v-icon></v-btn>
                </v-card-text>
            </v-card>
        </v-dialog>

    </div>
</template>

<script>

import S4DataSource from "@/plugins/S4DataSource";

export default {
    name: "NutritionTracker",
    data () {
        return {
            S4DataSource,
            show_food_booked: false,

            choosing_food_provider: false,
            food_provider: "",

            choosing_food_tags: false,
            food_tag_indices: [],

            entering_food_freetext: false,
            food_freetext_input: "",

            selected_food: false,

            selecting_food_amount: false,

            entering_preset_amount: false,
            proposed_preset_icon: "",
            proposed_preset_amount: 1,

            preset_idx: 0,
            
        };
    },
    computed: {
        food_amount: function () {
            return this.selected_food ? S4DataSource.amount_presets[this.selected_food.unit][this.preset_idx].amount : 0;
        },
        contains_macro: function () {
            return (this.selected_food.energy != 0) ||
                (this.selected_food.protein != 0) ||
                (this.selected_food.carbs != 0) ||
                (this.selected_food.fat != 0);
        },
        contains_vitamin: function () {
            return (this.selected_food.vit_A != 0) ||
                (this.selected_food.vit_B1 != 0) ||
                (this.selected_food.vit_B2 != 0) ||
                (this.selected_food.vit_B3 != 0) ||
                (this.selected_food.vit_B5 != 0) ||
                (this.selected_food.vit_B6 != 0) ||
                (this.selected_food.vit_B7 != 0) ||
                (this.selected_food.vit_B9 != 0) ||
                (this.selected_food.vit_B12 != 0) ||
                (this.selected_food.vit_C != 0) ||
                (this.selected_food.vit_D != 0) ||
                (this.selected_food.vit_E != 0) ||
                (this.selected_food.vit_K != 0);
        },
    },
    methods: {
        food_freetext_search: function () {
            alert("Would search for " + this.food_freetext_input);
            this.entering_food_freetext = false;
        },
        select_food: function(food) {
            this.selected_food = food;
            this.selecting_food_amount = true;
        },
        show_preset_amount_changer: function () {
            this.proposed_preset_icon = S4DataSource.amount_presets[this.selected_food.unit][this.preset_idx].icon;
            this.proposed_preset_amount = S4DataSource.amount_presets[this.selected_food.unit][this.preset_idx].amount;
            this.entering_preset_amount = true;
        },
        change_preset_amount: function () {
            S4DataSource.amount_presets[this.selected_food.unit][this.preset_idx].amount = this.proposed_preset_amount;
            this.entering_preset_amount = false;
        },
        selected_contents: function(what) {
            return this.selected_food ? Math.round(100 * this.food_amount * this.selected_food[what] / this.selected_food.reference_amount) / 100 : 0;
        },
        book_selected_food: function() {
            this.show_food_booked = true;
            this.selecting_food_amount = false;
        },
        debug: function() {
            alert("yadda");
        },
    },
    created: function() {
        console.log("NutTracker created");
    },
}

</script>

<style lang="scss" scoped>
@import "~vuetify/src/styles/main.sass";

    .nutrition-tracker {
        --food-filter-height: 44px;
        --active-tags-height: 24px;
        --food-list-header-height: 48px;
        --food-list-height: calc(var(--main-height) - var(--food-filter-height) - var(--active-tags-height) - var(--food-list-header-height));
    }

    .v-row {
        margin-bottom: 20px;
        &:last-child {
            margin-bottom: 0;
        }
    }

    .v-image.food-picker {
        color: white;
        caret-color: white;
        align-items: flex-end;
        height: 200px;
        contain: true;
        shaped: true;
        raised: true;
        padding: 10px;
    }

    .v-card__title {
        word-break: normal;
    }

    .v-data-table.food-table > .v-data-table__wrapper > table > tbody > tr > td {
        padding: 0;
    }



    #food-filter-tools {
        height: var(--food-filter-height);
    }

    #active-tags {
        max-height: var(--active-tags-height);
    }

    #food-list-header {
        height: var(--food-list-header-height);
    }

    #food-list {
        height: var(--food-list-height);
    }

    .food-card {
        max-width: 25vh;
        height: 40vh;
        background-color: map-get($amber, "lighten-2") !important;
    }
</style>

// vim: set sw=4 ts=4 et indk= :
