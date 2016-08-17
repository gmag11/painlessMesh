#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>
#include <easyMesh.h>
#include <easyWebSocket.h>

#include "animations.h"

NeoGamma<NeoGammaTableMethod> colorGamma; // for any fade animations, best to correct gamma
NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart800KbpsMethod> strip(PIXEL_COUNT, PIXEL_PIN);
NeoPixelAnimator animations(ANIMATION_COUNT); // NeoPixel animation management object
AnimationController controllers[ANIMATION_COUNT];

extern easyMesh mesh;

void animationsInit() {
  animations.StartAnimation(turnOnIdx, 3000, turnOn);
  controllers[turnOnIdx].dimmer = 0.1f;  // controls overall brightness 0-1f
  controllers[turnOnIdx].width = 3.0f; // width of the blip
  controllers[turnOnIdx].nextAnimation = searchingIdx;  // run once and then run search

  animations.StartAnimation(searchingIdx, 1000, searchingBlip);
  animations.StopAnimation(searchingIdx);
  controllers[searchingIdx].dimmer = 0.15f;  // controls overall brightness 0-1f
  controllers[searchingIdx].width = 2.0f; // width of the blip
  controllers[searchingIdx].hue[0] = 0.0f; // color of the blip
  controllers[searchingIdx].nextAnimation = searchingIdx;  // run itself over and over until this changes.

  animations.StartAnimation(smoothIdx, 2000, smoothBlip);
  animations.StopAnimation(smoothIdx);
  controllers[smoothIdx].dimmer = 0.1f;  // controls overall brightness 0-1f
  controllers[smoothIdx].width = 2.5f; // width of the blip

  controllers[smoothIdx].hue[0] = 0.22f; // color of the blip
  controllers[smoothIdx].hue[1] = 0.44f; // color of the blip
  controllers[smoothIdx].hue[2] = 0.66f; // color of the blip

  controllers[smoothIdx].offset = 0.0f;  // relative offset of this blip
  controllers[smoothIdx].direction = true; // true == clockwise, false == counterclockwise
  controllers[smoothIdx].nextAnimation = smoothIdx;  // run itself over and over until this changes.
}


void turnOn(const AnimationParam& param) {
  if (param.state == AnimationState_Completed) { // animation finished, restart
    animations.RestartAnimation(controllers[param.index].nextAnimation);
  }

  float lightness = controllers[param.index].dimmer;

  // fade in, fade out
  float fadeTime = 0.45f;
  if ( param.progress < fadeTime ) {          // fade in
    lightness *= param.progress / fadeTime;
  }
  if ( param.progress > ( 1 - fadeTime ) ) {  // fade out
    lightness *= ( 1 - param.progress ) / fadeTime;
  }

  // rotating rainbow
  float hue;
  float rotation;
  if ( param.progress > 0.5f )  //rotate clockwise
    rotation = param.progress * 4;
  else
    rotation = (1 - param.progress ) * 4; // rotate counter clockwise

  for (uint8_t index = 0 ; index < strip.PixelCount(); index++)
  {
    hue = (float)index / (float)strip.PixelCount();
    hue += rotation;

    while ( hue > 1) //kluge to get around lack of functioning fmod
      hue -= 1;

    RgbColor color = HslColor( hue, 1.0f, lightness);

    strip.SetPixelColor( index, colorGamma.Correct(color));
  }
}

void searchingBlip(const AnimationParam& param) {
  static uint8_t currentGoal, previousGoal;
  float blipPos;

  if (param.state == AnimationState_Completed) { // animation finished, restart
    previousGoal = currentGoal;
    currentGoal = ( currentGoal + strip.PixelCount() / 4 + random( strip.PixelCount() / 2 ) ) % strip.PixelCount();
    animations.RestartAnimation(controllers[param.index].nextAnimation);
  }

  float movementDuration = 0.3f;
  if ( param.progress < movementDuration )
    blipPos = abs( previousGoal + ( currentGoal - previousGoal ) * param.progress / movementDuration );
  else
    blipPos = (float)currentGoal;

  placeBlip( blipPos, controllers + param.index, 0 );
}

void smoothBlip(const AnimationParam& param) {
  uint16_t duration = animations.AnimationDuration(param.index);
  float progress = (float)( ( mesh.getNodeTime() / 1000 ) % duration ) / (float)duration;

  float offsetProgress = ( abs( controllers[param.index].direction - progress ) + controllers[param.index].offset);

  while ( offsetProgress > 1 ) // cheap float modulo 1
    offsetProgress -= 1;

  float blipPeak = offsetProgress * strip.PixelCount();

  if (param.state == AnimationState_Completed) { // animation finished, restart
    animations.RestartAnimation(controllers[param.index].nextAnimation);
  }

  allDark();

  uint16_t blips = mesh.connectionCount() + 1;
  if ( blips > MAX_BLIPS )
    blips = MAX_BLIPS;
  
  for ( int i = 0; i < blips; i++ ) {
    float delta = (float)i * (float)strip.PixelCount() / (float)blips;
    float tempPeak = blipPeak + delta;
    while ( tempPeak > strip.PixelCount() )  //hack float modulo
      tempPeak -= strip.PixelCount();

    placeBlip( tempPeak, controllers + param.index, i );
  }
}

void placeBlip(float& blipPos, AnimationController* controller, uint8_t hueIdx) {
  float lightness, sum, roldex;

  for (uint8_t index = 0 ; index < strip.PixelCount(); index++)
  {
    if ( blipPos + controller->width > strip.PixelCount() + index ) {  // if peak is within a width of the last pixel
      roldex = index + strip.PixelCount();
    }
    else if ( blipPos - controller->width < index - strip.PixelCount() ) {  // if peak is withing a width of the first pixel
      roldex = index - strip.PixelCount();
    }
    else {
      roldex = index;
    }

    sum = ( blipPos - roldex ) / controller->width;

    if ( 0 <= sum  && sum < 1 ) {
      lightness = 1 - sum;
    }
    else if ( 0 > sum && sum > -1 ) {
      lightness = sum + 1;
    }
    else {
      lightness = 0;
    }

    if ( lightness != 0 ) {
      lightness *= controller->dimmer;
      RgbColor color = HslColor( controller->hue[hueIdx], 1.0f, lightness);
      strip.SetPixelColor( index, colorGamma.Correct(color));
    }
  }
}

void allDark( void ) {
  RgbColor color = HslColor( 0, 1.0f, 0);

  for (uint8_t index = 0 ; index < strip.PixelCount(); index++) {
    strip.SetPixelColor( index, colorGamma.Correct(color));
  }
}

