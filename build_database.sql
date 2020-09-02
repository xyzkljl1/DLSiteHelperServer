-- --------------------------------------------------------
-- 主机:                           127.0.0.1
-- 服务器版本:                        8.0.17 - MySQL Community Server - GPL
-- 服务器OS:                        Win64
-- HeidiSQL 版本:                  10.2.0.5599
-- --------------------------------------------------------

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET NAMES utf8 */;
/*!50503 SET NAMES utf8mb4 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;


-- Dumping database structure for dlsitehelper
CREATE DATABASE IF NOT EXISTS `dlsitehelper` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */;
USE `dlsitehelper`;

-- Dumping structure for table dlsitehelper.overlap
CREATE TABLE IF NOT EXISTS `overlap` (
  `Main` varchar(50) NOT NULL,
  `Sub` varchar(50) NOT NULL,
  PRIMARY KEY (`Main`,`Sub`),
  KEY `SubWork` (`Sub`),
  CONSTRAINT `MainWork` FOREIGN KEY (`Main`) REFERENCES `works` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `SubWork` FOREIGN KEY (`Sub`) REFERENCES `works` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='内容覆盖关系，每条record表示Main的内容覆盖了Sub的所有内容';

-- Data exporting was unselected.

-- Dumping structure for table dlsitehelper.works
CREATE TABLE IF NOT EXISTS `works` (
  `id` varchar(50) NOT NULL,
  `eliminated` int(1) NOT NULL DEFAULT '0',
  `downloaded` int(1) NOT NULL DEFAULT '0',
  `bought` int(1) NOT NULL DEFAULT '0',
  `specialEliminated` int(1) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- Data exporting was unselected.

/*!40101 SET SQL_MODE=IFNULL(@OLD_SQL_MODE, '') */;
/*!40014 SET FOREIGN_KEY_CHECKS=IF(@OLD_FOREIGN_KEY_CHECKS IS NULL, 1, @OLD_FOREIGN_KEY_CHECKS) */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
