<h1 class="page-header">{{message}}</h1>



<?php 
  include_once '../includes/db_connect.php';
  include_once '../includes/functions.php';

  if (login_check($mysqli) == true) : ?>
  
  <p>
    Welcome <?php echo htmlentities($_SESSION['username']); ?>!
  </p>
  <p>
    Content will follow
  </p>
<?php else : ?>
    <p>
      <span class="error">You are not authorized to access this page.</span> Please <a href="../login.php">login</a>.
    </p>
<?php endif; ?>
