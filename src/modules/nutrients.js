
export default {
    state: {
        items: null,
    },
    getters: {
        nutrients(state, getters, rootState) { // eslint-disable-line no-unused-vars
            return state.items;
        },
    },
    mutations: {
    },
    actions: {
        fetch_nutrients(context) {
            return new Promise((resolve, reject) => {
                if (context.state.items == null) {
                    if (context.rootState.db != null) {
                        context.state.items = [];
                        context.rootState.db.executeSql("SELECT name, unit FROM Nutrient_t", [],
                            rs => {
                                console.log("Record count: " + rs.rows.length);
                                for (var x = 0; x < rs.rows.length; x++) {
                                    var item = rs.rows.item(x);
                                    console.log("Name: " + item.name + ", unit: " + item.unit);
                                    context.state.items.push(item);
                                }
                            },
                            error => {
                                reject(error); // SELECT failed
                            }
                        );
                    }
                    else {
                        context.state.items = [
                            { name: "sand", unit: "t" },
                            { name: "gasoline", unit: "l" },
                            { name: "dunno", unit: "dB/diopter" },
                        ];
                    }
                }
                resolve();
            });
        },
    },
};

// vim: set sw=4 ts=4 indk= et:
