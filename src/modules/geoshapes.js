import { latLng } from "leaflet";

var base_A = [ 0.003042,  0.002915 ];
var base_B = [ 0.002688, -0.003245 ];
const base_P = [ 25.113366, 55.169192 ];

const base_det = base_A[0] * base_B[1] - base_A[1] * base_B[0];

const base_Am = [ base_A[0] / base_det, base_A[1] / base_det ];
const base_Bm = [ base_B[0] / base_det, base_B[1] / base_det ];

function norm2latlng(p) {
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

function transform_base(lat, lng) {
    const p = [ lat - base_P[0], lng - base_P[1] ];

    var a =  p[0]*base_Bm[1] - p[1]*base_Bm[0];
    var b = -p[0]*base_Am[1] + p[1]*base_Am[0];

    a = (a >= 0) ? (a % 1) : 1 + (a % 1);
    b = (b >= 0) ? (b % 1) : 1 + (b % 1);

    return norm2latlng([a, b]);
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

const stations = {
    "A": {
        latLng: norm2latlng(scale(0.5, 0.5, 0.8, [0.0, 0.0])),
        ready: 4,
        charging: 3,
        free: 1,
    },
    "B": {
        latLng: norm2latlng(scale(0.5, 0.5, 0.8, [0.25, 0.5])),
        ready: 1,
        charging: 5,
        free: 2,
    },
    "C": {
        latLng: norm2latlng(scale(0.5, 0.5, 0.8, [1.0, 1.0])),
        ready: 0,
        charging: 2,
        free: 6,
    },
};

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

var current_location = latLng();

import { Plugins } from '@capacitor/core';
const { Geolocation } = Plugins;

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
            current_location = transform_base(loc.latitude, loc.longitude);
        }
    }

    var location_watcher = Geolocation.watchPosition({
        enableHighAccuracy: true,
    },watch_location);

    console.log("location_watcher=" + location_watcher);

    // Geolocation.clearWatch({id: location_watcher});
}
else {
    const sim_route = [
        { latitude: 25.04334, longitude: 55.24618, time: 2.0 },
        { latitude: 25.04334, longitude: 55.24633, time: 2.0 },
        { latitude: 25.04342, longitude: 55.24647, time: 2.0 },
        { latitude: 25.04353, longitude: 55.24652, time: 2.0 },
        { latitude: 25.04369, longitude: 55.24649, time: 2.0 },
        { latitude: 25.04381, longitude: 55.24642, time: 2.0 },
        { latitude: 25.04398, longitude: 55.24646, time: 2.0 },
        { latitude: 25.04413, longitude: 55.24652, time: 2.0 },
        { latitude: 25.04442, longitude: 55.24666, time: 2.0 },
        { latitude: 25.04472, longitude: 55.24681, time: 2.0 },
        { latitude: 25.04494, longitude: 55.24691, time: 2.0 },
        { latitude: 25.04515, longitude: 55.24702, time: 2.0 },
        { latitude: 25.04531, longitude: 55.24710, time: 2.0 },
        { latitude: 25.04538, longitude: 55.24721, time: 2.0 },
        { latitude: 25.04542, longitude: 55.24731, time: 2.0 },
        { latitude: 25.04550, longitude: 55.24739, time: 2.0 },
        { latitude: 25.04562, longitude: 55.24743, time: 2.0 },
        { latitude: 25.04570, longitude: 55.24741, time: 2.0 },
        { latitude: 25.04575, longitude: 55.24732, time: 2.0 },
        { latitude: 25.04575, longitude: 55.24723, time: 2.0 },
        { latitude: 25.04571, longitude: 55.24716, time: 2.0 },
        { latitude: 25.04564, longitude: 55.24714, time: 2.0 },
        { latitude: 25.04558, longitude: 55.24713, time: 2.0 },
        { latitude: 25.04553, longitude: 55.24715, time: 2.0 },
        { latitude: 25.04544, longitude: 55.24713, time: 2.0 },
        { latitude: 25.04531, longitude: 55.24708, time: 2.0 },
        { latitude: 25.04511, longitude: 55.24699, time: 2.0 },
        { latitude: 25.04469, longitude: 55.24677, time: 2.0 },
        { latitude: 25.04455, longitude: 55.24671, time: 2.0 },
        { latitude: 25.04440, longitude: 55.24664, time: 2.0 },
        { latitude: 25.04426, longitude: 55.24656, time: 2.0 },
        { latitude: 25.04409, longitude: 55.24646, time: 2.0 },
        { latitude: 25.04399, longitude: 55.24628, time: 2.0 },
        { latitude: 25.04389, longitude: 55.24613, time: 2.0 },
        { latitude: 25.04374, longitude: 55.24602, time: 2.0 },
        { latitude: 25.04361, longitude: 55.24598, time: 2.0 },
    ];

    var sim_route_idx = 0, sim_route_next_idx = 1;
    const sim_route_delta_t = 0.05;
    var sim_route_t = 0;
    var sim_latitude = 0, sim_longitude = 0; // eslint-disable-line no-unused-vars

    const sim_route_step = function () {
        sim_route_t += sim_route_delta_t;
        if (sim_route_t >= sim_route[sim_route_idx].time) {
            sim_route_t -= sim_route[sim_route_idx].time;
            sim_route_idx = sim_route_next_idx;
            sim_route_next_idx = (1 + sim_route_next_idx) % sim_route.length;
        }
        sim_latitude = (sim_route[sim_route_idx].latitude * (sim_route[sim_route_idx].time - sim_route_t) + sim_route[sim_route_next_idx].latitude * sim_route_t) / sim_route[sim_route_idx].time;
        sim_longitude = (sim_route[sim_route_idx].longitude * (sim_route[sim_route_idx].time - sim_route_t) + sim_route[sim_route_next_idx].longitude * sim_route_t) / sim_route[sim_route_idx].time;

        current_location = transform_base(sim_latitude, sim_longitude);
    }

    setInterval(sim_route_step, sim_route_delta_t * 1000);
}


export {
    norm2latlng,
    shops,
    stations,
    current_location,
};

// vim: set sw=4 ts=4 indk= et:

