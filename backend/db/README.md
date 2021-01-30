
## Create mongo db, service account, collection

```
use gps_tracker
db.createUser(
    {
        user: "backend",
        pwd: "zeihiwoofeim",
        roles: [ { role: "readWrite", db: "gps_tracker", }, ],
        authenticationRestrictions: [ { clientSource: [ "127.0.0.1/32" ], } ],
    },
)
db.createCollection("traces")
db.traces.createIndex( { unit: 1, time: -1 } )
db.traces.createIndex( { time: -1, unit: 1 } )


db.createCollection("sessions")

db.createCollection("stations")
mongoimport -c stations --jsonArray mongodb://backend:zeihiwoofeim@localhost:27017/gps_tracker ../../misc/get_osm_poi/dubai_cafe.json
```

## Commands

Connect to the db:
`mongo mongodb://backend:zeihiwoofeim@localhost:27017/gps_tracker`

(The rest goes within a mongo shell)

Insert data:
`db.traces.insert( { unit: "test-1", time: 0|(ISODate("2020-12-20T15:29:52.904Z") / 1000), lat: 25.04, lon: 55.25, alt: 30.11, battery: 100.0 })`

Where is the unit "test-1":
`db.traces.aggregate([{$match: {unit: "test-1"}}, {$sort: {time: -1}}, {$limit: 1}])`

The same:
`db.traces.find({unit: "test-1"}).sort({time: -1}).limit(1)`

Where are the units now:
`db.traces.aggregate([{$sort: {time: -1}}, {$group: {_id: "$unit", trace: {$first: "$$CURRENT"} }}, {$replaceRoot: { newRoot: "$trace"}} ])`








[//]: # ( vim: set sw=4 ts=4 et: )
