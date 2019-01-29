// Growing Roots State Machine
// pseudocode

// open qs: What happens if someone leaves during growth? ideal, growth retreats.
//

enum state
{
  noone,
  enter,
  grow,
  degrow
  burst,
};
state currentState = noone;
void setup()

{

}

void loop()
{
  // control machine
  switch (currentState)
  {
    case noone:
      // empty altar. wait until someone comes in.
      // if (someone) currentState = enter;
      break;
    case enter:
      // altar is occupied, wait for other altar as well.
      // if (otherAltarEnter) currentState = grow;
      break;
    case grow:
      // both altars start growing at the same time, until all road is taken.
      // if (lampCount reached) currentState = burst.
      // if someone leaves:
      // currentState = degrow;
      break;
    case degrow:
      // check all lights turned off, back to enter.
      // currentState = enter;
      break;
    case burst:
      // give it some time until overall burst appears and fadeout finishes.
      // if (millies-burstTime>5 sec) currentState = rest.
      break;
    case rest:
      // sleep for 5 mins.
      //if (millies-burstTime>300 sec) currentState = noone.
      break;
    default:
      break;
  }

  // update machine
  switch (currentState)
  {
    case noone:
      // front lights shining
      break;
    case enter:
      // turn off front lines. head light starts glow, waiting for other altar
      break;
    case grow:
      // other altar is also occupuied
      break;
    case degrow:
      // check all lights turned off, back to enter.
      // currentState = enter;
      break;
    case burst:
      // climax reached, fade out lights, send message to whole
      break;
    case rest:
      // sleep for 5 mins.
      break;
    default:
      break;
  }

}
