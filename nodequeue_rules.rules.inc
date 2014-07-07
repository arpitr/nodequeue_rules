<?php

/**
 * @file
 * Rules integration for the nodequeue module.
 */

// FIXES
// persist checkboxes value on rules
// Get Queues Name instead of machine name on cheeckboxes.

/**
  * Implements hook_rules_data_info().
  */
function nodequeue_rules_rules_data_info() {
  $return = array(
    'checkboxes' => array(
      'label' => t('checkboxes'),
      'ui class' => 'NodequeueRulesUIClass',
    )
  );
  return $return;
}

class NodequeueRulesUIClass extends RulesDataUI implements RulesDataDirectInputFormInterface {
  public static function getDefaultMode() {
    return 'input';
  }
  public static function inputForm($name, $info, $settings, RulesPlugin $element) {
    if (!empty($info['options list'])) {
      // Make sure the .rules.inc of the providing module is included as the
      // options list callback may reside there.
      $element->call('loadBasicInclude');
      $form[$name] = array(
        '#type' => 'checkboxes',
        '#options' => call_user_func($info['options list'], $element, $name),
      );
    }
    else {
      $form[$name] = array(
        '#type' => 'textarea',
      );
      RulesDataInputEvaluator::attachForm($form, $settings, $info, $element->availableVariables());
    }
        $settings += array($name => isset($info['default value']) ? $info['default value'] : NULL);
    $form[$name] += array(
      '#title' => t('Value'),
      '#default_value' => $settings[$name],
      '#required' => empty($info['optional']),
      '#after_build' => array('rules_ui_element_fix_empty_after_build'),
      '#rows' => 3,
    );
    return $form;
  }
    public static function render($value) {
    return array(
      'content' => array('#markup' => check_plain($value)),
      '#attributes' => array('class' => array('rules-parameter-text')),
    );
  }
}


/**
 * Implements hook_rules_action_info().
 */
function nodequeue_rules_rules_action_info() {
  return array(
    // Action: Add a given node to the given nodequeue.
    'nodequeue_rules_action_add_node' => array(
      'label' => t('Add node to nodequeue Bottom'),
      'group' => t('Nodequeue'),
      'parameter' => array(
        'node' => array(
          'type' => 'text',
          'label' => t('Node'),
          'description' => t('The node you want to add at the bottom of nodequeue.'),
        ),
        'queue' => array(
          'type' => 'checkboxes',
          'label' => t('Queue'),
          'description' => t('The queue where the node should be added to.'),
          'options list' => 'nodequeue_rules_get_queue_names',
        ),
        'unique' => array(
          'type' => 'boolean',
          'label' => t('Prevent duplication'),
          'description' => t('Selecting this option will prevent the node from being added more than once to any subqueue.'),
          'default value' => TRUE,
        )
      ),
    ),
    'nodequeue_rules_action_add_node_top' => array(
      'label' => t('Add node to nodequeue Top'),
      'group' => t('Nodequeue'),
      'parameter' => array(
        'node' => array(
          'type' => 'text',
          'label' => t('Node'),
          'description' => t('The node you want to add at the top of nodequue.'),
        ),
        'queue' => array(
          'type' => 'checkboxes',
          'label' => t('Queue'),
          'description' => t('The queue where the node should be added to.'),
          'options list' => 'nodequeue_rules_get_queue_names',
        ),
        'unique' => array(
          'type' => 'boolean',
          'label' => t('Prevent duplication'),
          'description' => t('Selecting this option will prevent the node from being added more than once to any subqueue.'),
          'default value' => TRUE,
        )
      ),
    ),
    // Action: Remove the given node from the given nodequeue.
    'nodequeue_rules_action_remove_node' => array(
      'label' => t('Remove node from nodequeue'),
      'group' => t('Nodequeue'),
      'parameter' => array(
        'node' => array(
          'type' => 'text',
          'label' => t('Node ID'),
          'description' => t('The node you want to remove.'),
        ),
        'queue' => array(
          'type' => 'checkboxes',
          'label' => t('Queue'),
          'description' => t('The queue where the node should be removed from.'),
          'options list' => 'nodequeue_rules_get_queue_names',
        ),
      ),
    ),
  );
}

/**
 * Action: Add the given node to the given nodequeue.
 */
function nodequeue_rules_action_add_node($node_id, $queue_name, $unique) {

  $node = node_load($node_id); 
  $queue = nodequeue_load_queues($queue_name);
  $subqueues = nodequeue_load_subqueues_by_queue($queue_name);
  foreach($subqueues as $subqueue) {
    $add = TRUE;
    // unique means we don't add it if it's in the queue already
    if ($unique) {
      $add = nodequeue_subqueue_position($subqueue->sqid, $node->nid) === FALSE;
    }
    if ($add) {
      if(_nodequeue_rules_node_add_check($node,$queue[$subqueue->name])) {
        nodequeue_subqueue_add($queue[$subqueue->name], $subqueue, $node->nid);
      }else {
        watchdog('nodequeue_rules', 'BOTTOM Node with @nid can not be added to nodequeue @nodequeue.', array('@nid' => $node_id, '@nodequeue' => $queue[$subqueue->name]->title), WATCHDOG_NOTICE);
      }
    }
  }
}

/**
 * Action: Add the given node to the given nodequeue.
 */
function nodequeue_rules_action_add_node_top($node_id, $queue_name, $unique) {
  $node = node_load($node_id);
  $queue = nodequeue_load_queues($queue_name);
  $subqueues = nodequeue_load_subqueues_by_queue($queue_name);
  foreach($subqueues as $subqueue) {
    $add = TRUE;
    // unique means we don't add it if it's in the queue already
    if ($unique) {
      $add = nodequeue_subqueue_position($subqueue->sqid, $node->nid) === FALSE;
    }
    if ($add) {
      if(_nodequeue_rules_node_add_check($node,$queue[$subqueue->name])) {
        nodequeue_subqueue_add_top($queue[$subqueue->name], $subqueue, $node->nid);
      }else {
        watchdog('nodequeue_rules', ' TOP Node with @nid can not be added to nodequeue @nodequeue.', array('@nid' => $node_id, '@nodequeue' => $queue[$subqueue->name]->title), WATCHDOG_NOTICE);
      }
    }
  }
}

/**
  * Helper function to check if node can be added to nodequeue
  * Check is on the basis of conten type.
  */
function _nodequeue_rules_node_add_check($node,$queue) {
  $check = FALSE;
  // Get node type.
  $node_type = $node->type;

  // Get nodequeue type.
  $queue_type = $queue->types;

  if(count($queue_type) == 0) {
    $check = TRUE;
  } elseif(in_array($node_type,$queue_type)) {
    $check = TRUE;
  }
  return $check; 
}


/**
 * Action: Remove the given node from the given nodequeue.
 * TO DO: Check if node exists or not.
 */
function nodequeue_rules_action_remove_node($node_id, $queue_name) {
  $node = node_load($node_id);
  if(is_object($node) && isset($node->nid)) {
    $queue = nodequeue_load_queues($queue_name);
    $subqueues = nodequeue_load_subqueues_by_queue($queue_name);
    foreach ($subqueues as $subqueue) {
      if(nodequeue_subqueue_position($subqueue->sqid,$node_id)) {
        if (!$queue[$subqueue->name]->i18n || (isset($node->tnid) && empty($node->tnid))) {
          nodequeue_subqueue_remove_node($subqueue->sqid, $node->nid);
        }
      } else {
        watchdog('nodequeue_rules', 'Node with @nid does not exist in nodequeue @nodequeue.', array('@nid' => $node_id, '@nodequeue' => $subqueue->name), WATCHDOG_NOTICE);
      }
    }
  }
}

/**
 * Return a list of nodequeue names.
 */
function nodequeue_rules_get_queue_names() {
  $items = array();
  $items = _nodequeue_rules_get_all_queues();
  return $items;
}

/**
  * Need doc.
  */
function _nodequeue_rules_get_all_queues() {
  $query = db_select('nodequeue_queue', 'nq')
    ->fields('nq', array('name','title'));
  $queues = $query->execute()->fetchAllKeyed(0, 1);
  return $queues;
}



/**
 * Helper function for fetching possible domain ids for a node.
 */
function nodequeue_rules_get_node_domains($node){
  $node_domains = array();
  if(empty($node->nid) || !function_exists('domain_get_node_domains')){
    return $node_domains;
  }
  $domain_data = domain_get_node_domains($node->nid);
  if(!empty($domain_data['domain_id'])){
    return $domain_data['domain_id'];
  }
  return $node_domains;
}