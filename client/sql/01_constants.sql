------------------------------------------------------------------------------
-- Global key-value store
--

CREATE TABLE Global_t (
    name            VARCHAR(32) NOT NULL PRIMARY KEY,
    value           VARCHAR(64) NOT NULL,
    last_modified   TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO Global_t (name, value) VALUES
	("constants", "");  -- just use the last_modified time marker

------------------------------------------------------------------------------
-- Language
--
-- ISO 639-3
--

CREATE TABLE Language_t (
    id              CHAR(3) NOT NULL PRIMARY KEY,
    name            VARCHAR(32)
);

INSERT INTO Language_t (id, name) VALUES
    ("eng", "English"),
    ("ara", "Arabic"),
    ("hin", "Hindi"),
    ("urd", "Urdu"),
    ("fil", "Filipino"),
    ("mly", "Malay");


------------------------------------------------------------------------------
-- Religion
--
-- For holiday greetings, for Ramadan timing considerations, etc.
--
-- Rarely referred to -> enum-style
-- FIXME: top 4 added, order by population desc, expand if needed
--

CREATE TABLE Religion_t (
    name            VARCHAR(16) PRIMARY KEY
);

INSERT INTO Religion_t (name) VALUES
    ("Christianity"),
    ("Islam"),
    ("Hinduism"),
    ("Buddhism");


------------------------------------------------------------------------------
-- Country
--
-- ISO 3166-2: https://en.wikipedia.org/wiki/ISO_3166-2
--

CREATE TABLE Country_t (
    id              CHAR(2) NOT NULL PRIMARY KEY,
    name            VARCHAR(32),
    long_name       VARCHAR(64)
);

INSERT INTO Country_t (id, name) VALUES
    ("AE",  "United Arab Emirates"),
    ("BH",  "Bahrain"),
    ("KW",  "Kuwait"),
    ("OM",  "Oman"),
    ("QA",  "Qatar"),
    ("SA",  "Saudi Arabia");

CREATE UNIQUE INDEX Country_by_name_i
	ON Country_t (name);


------------------------------------------------------------------------------
-- Subdivision (State/Province)
--
-- ISO 3166-2:<xx>
--

CREATE TABLE State_t (
    id              VARCHAR(3) NOT NULL,
    country         CHAR(2) NOT NULL REFERENCES Country_t,
    name            VARCHAR(32),
    long_name       VARCHAR(64),
    PRIMARY KEY (country, id)
);

INSERT INTO State_t (country, id, name) VALUES
    ("AE",  "AZ",   "Abu Dhabi"),
    ("AE",  "AJ",   "Ajman"),
    ("AE",  "FU",   "Fujairah"),
    ("AE",  "SH",   "Sharjah"),
    ("AE",  "DU",   "Dubai"),
    ("AE",  "RK",   "Ras Al Khaimah"),
    ("AE",  "UQ",   "Umm Al Quwain"),
    ("BH",  "13",   "Al Manamah"),
    ("BH",  "14",   "Al Janubiyah"),
    ("BH",  "15",   "Al Muharraq"),
    ("BH",  "17",   "Al Shamaliyah"),
    ("KW",  "AH",   "Al Ahmadi"),
    ("KW",  "FA",   "Al Farwaniyah"),
    ("KW",  "JA",   "Al Jahra"),
    ("KW",  "KU",   "Al Kuwayt"),
    ("KW",  "HA",   "Hawalli"),
    ("KW",  "MU",   "Mubarak Al Kabir"),
    ("OM",  "DA",   "Al Dakhiliya"),
    ("OM",  "BS",   "Shamal Al Batinah"),
    ("OM",  "BJ",   "Janub Al Batinah"),
    ("OM",  "WU",   "Al Wusta"),
    ("OM",  "SS",   "Shamal Al Sharqiyah"),
    ("OM",  "SJ",   "Janub Al Sharqiyah"),
    ("OM",  "ZA",   "Al Zahirah"),
    ("OM",  "BU",   "Al Buraymi"),
    ("OM",  "MA",   "Muscat"),
    ("OM",  "MU",   "Musandam"),
    ("OM",  "ZU",   "Dhofar"),
    ("QA",  "DA",   "Al Dawhah"),
    ("QA",  "KH",   "Al Khawr wa Al Dhakhirah"),
    ("QA",  "WA",   "Al Wakrah"),
    ("QA",  "RA",   "Al Rayyan"),
    ("QA",  "MS",   "Al Shamal"),
    ("QA",  "SH",   "Al Shihaniyah"),
    ("QA",  "ZA",   "Al Za'ayin"),
    ("QA",  "US",   "Umm Salal"),
    ("SA",  "11",   "Al Bahah"),
    ("SA",  "08",   "Al Hudud Al Shamaliyah"),
    ("SA",  "12",   "Al Jawf"),
    ("SA",  "03",   "Al Madinah Al Mudawwarah"),
    ("SA",  "05",   "Al Qasim"),
    ("SA",  "01",   "Al Riyad"),
    ("SA",  "04",   "Al Sharqiyah"),
    ("SA",  "14",   "'Asir"),
    ("SA",  "06",   "Ha'il"),
    ("SA",  "09",   "Jazan"),
    ("SA",  "02",   "Makkah Al Mukarramah"),
    ("SA",  "10",   "Najran"),
    ("SA",  "07",   "Tabuk");

CREATE UNIQUE INDEX State_by_country_and_name_i
	ON State_t (country, name);


------------------------------------------------------------------------------
-- District
--
-- Part of a state, like "Motor City" or "JLT"
--

CREATE TABLE District_t (
    id              INTEGER AUTO_INCREMENT PRIMARY KEY,
    name            VARCHAR(32),
    long_name       VARCHAR(64),
    country         VARCHAR(2) NOT NULL,
    state           VARCHAR(3) NOT NULL,
    latitude        FLOAT,
    longitude       FLOAT,
    radius          FLOAT,
    FOREIGN KEY (country, state) REFERENCES State_t (country, id)
);

-- Create a "*" district for all states
INSERT INTO District_t (country, state, name) SELECT country, id, "*" FROM State_t;
INSERT INTO District_t (country, state, name) VALUES
    ("AE", "DU", "Abu Hail"),
    ("AE", "DU", "Al Awir"),
    ("AE", "DU", "Al Bada"),
    ("AE", "DU", "Al Barsha"),
    ("AE", "DU", "Al Buteen"),
    ("AE", "DU", "Al Corniche "),
    ("AE", "DU", "Al Dhagaya"),
    ("AE", "DU", "Aleyas"),
    ("AE", "DU", "Al Garhoud"),
    ("AE", "DU", "Al Hamriya"),
    ("AE", "DU", "Al Hudaiba"),
    ("AE", "DU", "Al Jaddaf"),
    ("AE", "DU", "Al Jafiliya"),
    ("AE", "DU", "Al Karama"),
    ("AE", "DU", "Al Khabisi"),
    ("AE", "DU", "Al Khwaneej"),
    ("AE", "DU", "Al Kifaf"),
    ("AE", "DU", "Al Mamzar"),
    ("AE", "DU", "Al Manara"),
    ("AE", "DU", "Al Mankhool"),
    ("AE", "DU", "Al Merkad"),
    ("AE", "DU", "Al Mina"),
    ("AE", "DU", "Al Mizhar"),
    ("AE", "DU", "Al Muraqqabat"),
    ("AE", "DU", "Al Murar"),
    ("AE", "DU", "Al Mushrif"),
    ("AE", "DU", "Al Muteena"),
    ("AE", "DU", "Al Nahda"),
    ("AE", "DU", "Al Nasr"),
    ("AE", "DU", "Al Quoz"),
    ("AE", "DU", "Al Qusais"),
    ("AE", "DU", "Al Raffa"),
    ("AE", "DU", "Al Ras"),
    ("AE", "DU", "Al Rashidiya"),
    ("AE", "DU", "Al Rigga"),
    ("AE", "DU", "Al Ruwayya"),
    ("AE", "DU", "Al Sabkha"),
    ("AE", "DU", "Al Safa"),
    ("AE", "DU", "Al Safouh"),
    ("AE", "DU", "Al Satwa"),
    ("AE", "DU", "Al Shindagha"),
    ("AE", "DU", "Al Souq Al Kabeer"),
    ("AE", "DU", "Al Twar"),
    ("AE", "DU", "Al Waheda"),
    ("AE", "DU", "Al Warqa'a"),
    ("AE", "DU", "Al Wasl"),
    ("AE", "DU", "Arabian Ranches"),
    ("AE", "DU", "Ayal Nasir"),
    ("AE", "DU", "Bu Kadra"),
    ("AE", "DU", "Business Bay"),
    ("AE", "DU", "Corniche Deira"),
    ("AE", "DU", "Downtown Burj Dubai"),
    ("AE", "DU", "Downtown Jebel Ali"),
    ("AE", "DU", "DXB Airport"),
    ("AE", "DU", "Emirates Hill"),
    ("AE", "DU", "Festival City"),
    ("AE", "DU", "Hatta"),
    ("AE", "DU", "Hor Al Anz"),
    ("AE", "DU", "International City"),
    ("AE", "DU", "Internet City"),
    ("AE", "DU", "Investment Park"),
    ("AE", "DU", "Jebel Ali"),
    ("AE", "DU", "Jebel Ali Conservation Area"),
    ("AE", "DU", "Jebel Ali Free Zone"),
    ("AE", "DU", "Jebel Ali Industrial"),
    ("AE", "DU", "Jebel Ali Village"),
    ("AE", "DU", "Jumeira"),
    ("AE", "DU", "Jumeira Lakes Towers"),
    ("AE", "DU", "Majan"),
    ("AE", "DU", "Marina"),
    ("AE", "DU", "Maritime City"),
    ("AE", "DU", "Marsa Dubai"),
    ("AE", "DU", "Mirdif"),
    ("AE", "DU", "Motor City"),
    ("AE", "DU", "Muhaisanah"),
    ("AE", "DU", "Nad Al Hammar"),
    ("AE", "DU", "Nadd Al Shiba"),
    ("AE", "DU", "Nad Shamma"),
    ("AE", "DU", "Naif"),
    ("AE", "DU", "Nakhlat Deira"),
    ("AE", "DU", "Nakhlat Jebel Ali"),
    ("AE", "DU", "Nakhlat Jumeira"),
    ("AE", "DU", "Oud Al Muteena"),
    ("AE", "DU", "Oud Metha"),
    ("AE", "DU", "Palm Deira Jebel Ali"),
    ("AE", "DU", "Port Rashid"),
    ("AE", "DU", "Port Saeed"),
    ("AE", "DU", "Ras Al Khor"),
    ("AE", "DU", "Rigga Al Buteen"),
    ("AE", "DU", "Sector 63"),
    ("AE", "DU", "Sector 64"),
    ("AE", "DU", "Sector 65"),
    ("AE", "DU", "Silicon Oasis"),
    ("AE", "DU", "Trade Centre"),
    ("AE", "DU", "Umm Al Sheif"),
    ("AE", "DU", "Umm Hurair"),
    ("AE", "DU", "Umm Ramool"),
    ("AE", "DU", "Umm Suqeim"),
    ("AE", "DU", "University Village"),
    ("AE", "DU", "Wadi Alamardi"),
    ("AE", "DU", "Warsan"),
    ("AE", "DU", "Za'abeel");


------------------------------------------------------------------------------
-- Title
--
-- For addressing/greeting persons
--
-- Rarely referred to -> enum-style
-- FIXME: add nobility titles, military ranks, etc. (-> customer satisfaction)
--

CREATE TABLE Title_t (
    value           VARCHAR(16) PRIMARY KEY
);

INSERT INTO Title_t (value) VALUES
    ("Ms."),
    ("Mrs."),
    ("Mr."),
    ("Dr."),
    ("Prof.");


------------------------------------------------------------------------------
-- Nutrients
--
-- Supplements / foods may contain them, clients have diet goals for them
--
-- Needs compact ID because personal goals and diet tracker will refer to it
--
-- Energy content:
-- fats: 8.8 kcal/g
-- alcohol: 6.9 kcal/g
-- proteins, carbohydrates: 4 kcal/g
-- polyols: 2.4 kcal/g -- like sweeteners
-- organic acids: 3.1 kcal/g -- take the dilution into account!
-- fiber: 2 kcal/g
--

CREATE TABLE Nutrient_t (
    id              INTEGER AUTO_INCREMENT PRIMARY KEY,
    name            VARCHAR(32) NOT NULL,
    unit            VARCHAR(8) NOT NULL -- mg, kcal, g, etc.
);

INSERT INTO Nutrient_t (name, unit) VALUES
    -- Not nutrients, but countable
    ("Energy",                  "kcal"),
    ("Water",                   "ml"),
    -- Protein
    ("Protein",                 "g"),
    -- Fats
    ("Saturated fat",           "g"),
    ("Monounsaturated fat",     "g"), -- (omega-7 and -7)
    ("Polyunsaturated fat",     "g"), -- (omega-3 and -6)
    ("Trans-unsaturated fat",   "g"), -- (also unsaturated)
    -- Carbohydrates
    ("Sugar",                   "g"), -- (simple carbohydrates: mono- and disaccharides)
    ("Carbs",                   "g"), -- (digestible complex carbohydrates)
    ("Fiber",                   "g"), -- (indigestible complex carbohydrates)
    -- Trace minerals
    ("Cr",                      "mg"),
    ("Co",                      "mg"),
    ("Cu",                      "mg"),
    ("I",                       "mg"),
    ("Fe",                      "mg"),
    ("Mn",                      "mg"),
    ("Mo",                      "mg"),
    ("Se",                      "mg"),
    ("Zn",                      "mg"),
    ("Mg",                      "mg"),
    ("Ca",                      "mg"),
    ("K",                       "mg"),
    ("Na",                      "mg"),
    ("P" ,                      "mg"),
    -- Vitamins
    ("Vitamin A",               "mg"),
    ("Vitamin B1",              "mg"), -- (Thiamine)
    ("Vitamin B2",              "mg"), -- (Riboflavin)
    ("Vitamin B3",              "mg"), -- (Niacin)
    ("Vitamin B5",              "mg"), -- (Panthotenic acid)
    ("Vitamin B6",              "mg"), -- (Pyridoxine)
    ("Vitamin B7",              "mg"), -- (Biotin)
    ("Vitamin B9",              "mg"), -- (Folic acid)
    ("Vitamin B12",             "mg"), -- (Cobalamin)
    ("Vitamin C",               "mg"), -- (Ascorbic acid)
    ("Vitamin D",               "mg"), -- (Cholecalciferol)
    ("Vitamin E",               "mg"), -- (Tocopherol)
    ("Vitamin K ",              "mg"), -- (Quinone)
    -- Supplements
    ("EPA",                     "g"), -- (a type of omega-3)
    ("DHA",                     "g"), -- (a type of omega-3)
    ("L-Carnitine",             "g"),
    ("L-Glutamine",             "g"),
    ("L-Arginine",              "g"),
    ("Creatin",                 "g"),
    ("Leucine",                 "g"), -- (a type of BCAA)
    ("Isoleucine",              "g"), -- (a type of BCAA)
    ("Valine",                  "g"), -- (a type of BCAA)
    ("BCAA",                    "g"), -- (other/nonspecified BCAA)
    ("Casein",                  "g"),
    -- Other
    ("Caffeine",                "g"),
    ("Taurin",                  "g");


CREATE INDEX Nutrient_by_name_i
    ON Nutrient_t (name);


------------------------------------------------------------------------------
-- Allergens
--
-- Represented as a 32-bit bitfield:
-- Gluten       0x00000001
-- Shellfish    0x00000002
-- Egg          0x00000004
-- Fish         0x00000008
-- Peanut       0x00000010
-- Soy          0x00000020
-- Milk         0x00000040
-- Lactose      0x00000080
-- Nuts         0x00000100
-- Celery       0x00000200
-- Mustard      0x00000400
-- Sesame       0x00000800
-- Sulphites    0x00001000
-- Haram        0x80000000

------------------------------------------------------------------------------

CREATE TABLE Allergen_t (
    id          INTEGER NOT NULL PRIMARY KEY,
    name        VARCHAR(32) NOT NULL
);

INSERT INTO Allergen_t (id, name) VALUES
    (0x00000001, "Gluten"),
    (0x00000002, "Shellfish"),
    (0x00000004, "Egg"),
    (0x00000008, "Fish"),
    (0x00000010, "Peanut"),
    (0x00000020, "Soy"),
    (0x00000040, "Milk"),
    (0x00000080, "Lactose"),
    (0x00000100, "Nuts"),
    (0x00000200, "Celery"),
    (0x00000400, "Mustard"),
    (0x00000800, "Sesame"),
    (0x00001000, "Sulphites"),
    (0x20000000, "Meat"),
    (0x40000000, "Haram");


UPDATE Global_t SET last_modified=CURRENT_TIMESTAMP WHERE name="constants";

-- vim: set sw=4 ts=4 et indk= :
