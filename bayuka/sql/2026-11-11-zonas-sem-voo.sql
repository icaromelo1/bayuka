DELETE FROM `spell_area` WHERE `spell` = 58600 AND `area` IN (1497, 1519, 1537, 1637, 1638, 1657, 3487, 3557);
INSERT INTO `spell_area` (`spell`, `area`, `quest_start`, `quest_end`, `aura_spell`, `racemask`, `gender`, `autocast`, `quest_start_status`, `quest_end_status`) VALUES
(58600, 1497, 0, 0, 0, 0, 2, 1, 64, 11),
(58600, 1519, 0, 0, 0, 0, 2, 1, 64, 11),
(58600, 1537, 0, 0, 0, 0, 2, 1, 64, 11),
(58600, 1637, 0, 0, 0, 0, 2, 1, 64, 11),
(58600, 1638, 0, 0, 0, 0, 2, 1, 64, 11),
(58600, 1657, 0, 0, 0, 0, 2, 1, 64, 11),
(58600, 3487, 0, 0, 0, 0, 2, 1, 64, 11),
(58600, 3557, 0, 0, 0, 0, 2, 1, 64, 11);
