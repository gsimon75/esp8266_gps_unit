import { latLng } from "leaflet";

var router, store;

function register_context(r, s) {
    router = r;
    store = s;
    console.log("geo registered vue context");
}

var base_A = [ 0.003042,  0.002915 ];
var base_B = [ 0.002688, -0.003245 ];
const base_P = [ 25.113366, 55.169192 ];

function norm2latlng(p) {
    p = scale(0.5, 0.5, 0.8, p);
    return latLng(base_P[0] + p[0]*base_A[0] + p[1]*base_B[0], base_P[1] + p[0]*base_A[1] + p[1]*base_B[1]);
}

function norm2lnglat(p) {
    return [
        base_P[1] + p[0]*base_A[1] + p[1]*base_B[1],
        base_P[0] + p[0]*base_A[0] + p[1]*base_B[0],
    ];
}

function scale(cx, cy, m, p) {
    return [ cx + (p[0] - cx) * m, cy + (p[1] - cy) * m ];
}

const shop_coords = [
    [ [0.43, 0.45], [0.25, 0.4], [0.25, 0.25], [0.43, 0.3], ],
    [ [0.25, 0.4], [0.15, 0.45], [0.025, 0.4], [0.12, 0.3], [0.25, 0.25], ],
    [ [0.15, 0.45], [0.15, 0.55], [0.025, 0.6], [0, 0.5], [0.025, 0.4], ],
    [ [0.15, 0.55], [0.25, 0.6], [0.2, 0.7], [0.1, 0.75], [0.025, 0.6], ],
    [ [0.25, 0.6], [0.43, 0.55], [0.43, 0.7], [0.3, 0.73], [0.2, 0.7], ],
    [ [0.2, 0.7], [0.35, 0.75], [0.25, 0.9], [0.1, 0.75], ],
    [ [0.43, 0.7], [0.5, 0.75], [0.46, 0.85], [0.35, 0.75], [0.3, 0.73], ],
    [ [0.35, 0.75], [0.46, 0.85], [0.5, 0.9], [0.45, 1.0], [0.25, 0.9], ],
    [ [0.5, 0.75], [0.7, 0.8], [0.7, 0.9], [0.5, 0.9], [0.46, 0.85], ],
    [ [0.5, 0.9], [0.7, 0.9], [0.7, 0.95], [0.45, 1.0], ],
    [ [0.7, 0.8], [0.9, 0.75], [0.7, 0.95], ],
];

const stations = [
    {
        id: "Alpha",
        loc: norm2latlng([0.0, 0.0]),
        ready: 4,
        charging: 3,
        free: 1,
    },
    {
        id: "Beta",
        loc: norm2latlng([0.25, 0.5]),
        ready: 1,
        charging: 5,
        free: 2,
    },
    {
        id: "Gamma",
        loc: norm2latlng([1.0, 1.0]),
        ready: 0,
        charging: 2,
        free: 6,
    },
];

function nearest_station(eligible_stations) {
    if (eligible_stations.length == 0) {
        return undefined;
    }
    return eligible_stations
        .map(st => { return { ...st, d: st.loc.distanceTo(store.state.current_location)}; })
        .reduce((best, st) => (st.d < best.d) ? st : best);
}

const shops = {
    type: "FeatureCollection",
    features: [],
};

for (let i in shop_coords) {
    
    shops.features.push({
        type: "Feature",
        geometry: {
            type: "Polygon",
            coordinates: [ shop_coords[i].map(c => norm2lnglat(scale(0.5, 0.5, 0.8, [ c[0], c[1] ]))) ],
        },
        properties: {
            id: "A" + (1 + i),
            color: "#ff4000",
            fill: true,
            fillColor: "#ff0000",
            fillOpacity: 0.2,
        },
    });

    shops.features.push({
        type: "Feature",
        geometry: {
            type: "Polygon",
            coordinates: [ shop_coords[i].map(c => norm2lnglat(scale(0.5, 0.5, 0.8, [ 1 - c[0], 1 - c[1] ]))) ],
        },
        properties: {
            id: "B" + (1 + i),
            color: "#0040ff",
            fill: true,
            fillColor: "#0000ff",
            fillOpacity: 0.2,
        },
    });

}

/*shops.features.push({
    type: "Feature",
    geometry: {
        type: "Polygon",
        coordinates: [ [
            norm2lnglat([0, 0]),
            norm2lnglat([1, 0]),
            norm2lnglat([1, 1]),
            norm2lnglat([0, 1]),
            norm2lnglat([0, 0]),
        ] ],
    },
    properties: {
        color: "#ffffff",
    },
});*/

/*shops.features.push({
    type: "Feature",
    geometry: {
        type: "MultiLineString",
        coordinates: [
            [
                norm2lnglat(scale(0.5, 0.5, 0.8, [0.25, 0.25])),
                norm2lnglat(scale(0.5, 0.5, 0.8, [0.25, 0.75])),
            ],
            [
                norm2lnglat(scale(0.5, 0.5, 0.8, [0, 0.5])),
                norm2lnglat(scale(0.5, 0.5, 0.8, [0.5, 0.5])),
            ],
        ],
    },
    properties: {
        color: "#ffffff",
    },
});*/

/*import { Plugins } from '@capacitor/core';
const { Geolocation } = Plugins;

const base_det = base_A[0] * base_B[1] - base_A[1] * base_B[0];
const base_Am = [ base_A[0] / base_det, base_A[1] / base_det ];
const base_Bm = [ base_B[0] / base_det, base_B[1] / base_det ];

function transform_base(lat, lng) {
    const p = [ lat - base_P[0], lng - base_P[1] ];

    var a =  p[0]*base_Bm[1] - p[1]*base_Bm[0];
    var b = -p[0]*base_Am[1] + p[1]*base_Am[0];

    a = (a >= 0) ? (a % 1) : 1 + (a % 1);
    b = (b >= 0) ? (b % 1) : 1 + (b % 1);

    return norm2latlng([a, b]);
}

if (typeof cordova !== "undefined") {
    // https://capacitorjs.com/docs/apis/geolocation#geolocationposition


    const watch_location = function (loc, err) {
        if (err) {
            console.log("watch_location err=" + err);
        }
        else {
            console.log("loc.timestamp=" + loc.timestamp);
            console.log("loc.latitude=" + loc.latitude);
            console.log("loc.longitude=" + loc.longitude);
            store.commit("set_location", transform_base(loc.latitude, loc.longitude));
        }
    }

    var location_watcher = Geolocation.watchPosition({
        enableHighAccuracy: true,
    },watch_location);

    console.log("location_watcher=" + location_watcher);

    // Geolocation.clearWatch({id: location_watcher});
}
else*/ {
    const sim_route = [
        { loc: norm2latlng([ 0, 0]), time: 4.0 },
        { loc: norm2latlng([ 0, 0.25]), time: 2.0 },
        { loc: norm2latlng([ 0.1, 0.3]), time: 2.0 },
        { loc: norm2latlng([ 0.25, 0.2]), time: 2.0 },
        { loc: norm2latlng([ 0.45, 0.25]), time: 2.0 },
        { loc: norm2latlng([ 0.5, 0.3]), time: 2.0 },
        { loc: norm2latlng([ 0.5, 0.4]), time: 2.0 },
        { loc: norm2latlng([ 0.45, 0.5]), time: 2.0 },
        { loc: norm2latlng([ 0.25, 0.5]), time: 2.0 },
        { loc: norm2latlng([ 0.25, 0.5]), time: 4.0 },
        { loc: norm2latlng([ 0.5, 0.5]), time: 2.0 },
        { loc: norm2latlng([ 0.75, 0.5]), time: 2.0 },
        { loc: norm2latlng([ 0.55, 0.5]), time: 2.0 },
        { loc: norm2latlng([ 0.5, 0.6]), time: 2.0 },
        { loc: norm2latlng([ 0.6, 0.75]), time: 2.0 },
        { loc: norm2latlng([ 0.75, 0.75]), time: 2.0 },
        { loc: norm2latlng([ 0.9, 0.7]), time: 2.0 },
        { loc: norm2latlng([ 1, 0.8]), time: 2.0 },
        { loc: norm2latlng([ 1, 1]), time: 2.0 },
        { loc: norm2latlng([ 1, 1]), time: 4.0 },
        { loc: norm2latlng([ 0.5, 1.1]), time: 6.0 },
        { loc: norm2latlng([ 0, 1]), time: 6.0 },
        { loc: norm2latlng([ -0.1, 0.5]), time: 6.0 },
        { loc: norm2latlng([ 0, 0]), time: 6.0 },
    ];

    var sim_route_idx = 0, sim_route_next_idx = 1;
    const sim_route_delta_t = 0.2;
    var sim_route_t = 0;

    const sim_route_step = function () {
        if (router.currentRoute.fullPath == "/site_map") {
            sim_route_t += sim_route_delta_t;
            if (sim_route_t >= sim_route[sim_route_idx].time) {
                sim_route_t -= sim_route[sim_route_idx].time;
                sim_route_idx = sim_route_next_idx;
                sim_route_next_idx = (1 + sim_route_next_idx) % sim_route.length;
            }
            const sim_latitude = (sim_route[sim_route_idx].loc.lat * (sim_route[sim_route_idx].time - sim_route_t) + sim_route[sim_route_next_idx].loc.lat * sim_route_t) / sim_route[sim_route_idx].time;
            const sim_longitude = (sim_route[sim_route_idx].loc.lng * (sim_route[sim_route_idx].time - sim_route_t) + sim_route[sim_route_next_idx].loc.lng * sim_route_t) / sim_route[sim_route_idx].time;

            store.commit("set_location", latLng(sim_latitude, sim_longitude));
        }
    }

    setInterval(sim_route_step, sim_route_delta_t * 1000);
}

export {
    register_context,
    norm2latlng,
    nearest_station,
    shops,
    stations,
};

// vim: set sw=4 ts=4 indk= et:

