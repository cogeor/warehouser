/**
 * useRosService - React hooks for calling ROS services
 *
 * Provides hooks for calling ROS services using the new connection system.
 */

import { useCallback } from 'react';
import ROSLIB from 'roslib';
import { useRosConnection } from './useRosConnection';

/**
 * Hook to call a ROS service.
 *
 * @param serviceName - The name of the ROS service
 * @param serviceType - The ROS service type string
 * @returns A function to call the service
 */
export function useRosService<TRequest, TResponse>(
  serviceName: string,
  serviceType: string
): (request: TRequest) => Promise<TResponse | null> {
  const { ros, isConnected } = useRosConnection();

  const callService = useCallback(
    async (request: TRequest): Promise<TResponse | null> => {
      if (!isConnected || ros === null) {
        console.warn(`Cannot call service ${serviceName}: not connected`);
        return null;
      }

      return new Promise((resolve) => {
        const service = new ROSLIB.Service({
          ros,
          name: serviceName,
          serviceType,
        });

        // Timeout after 5 seconds
        const timeout = setTimeout(() => {
          console.warn(`Service ${serviceName} timed out`);
          resolve(null);
        }, 5000);

        service.callService(
          new ROSLIB.ServiceRequest(request as object),
          (response: unknown) => {
            clearTimeout(timeout);
            resolve(response as TResponse);
          }
        );
      });
    },
    [ros, isConnected, serviceName, serviceType]
  );

  return callService;
}

/**
 * Hook to call a Trigger service (std_srvs/srv/Trigger).
 *
 * @param serviceName - The name of the ROS service
 * @returns A function to call the service, returns success boolean
 */
export function useTriggerService(
  serviceName: string
): () => Promise<boolean> {
  const callService = useRosService<object, { success: boolean }>(
    serviceName,
    'std_srvs/srv/Trigger'
  );

  return useCallback(async () => {
    const response = await callService({});
    return response?.success ?? false;
  }, [callService]);
}

/**
 * Hook to publish to a ROS topic.
 *
 * @param topicName - The name of the ROS topic
 * @param messageType - The ROS message type string
 * @returns A function to publish messages
 */
export function useRosPublisher<T>(
  topicName: string,
  messageType: string
): (message: T) => void {
  const { ros, isConnected } = useRosConnection();

  const publish = useCallback(
    (message: T): void => {
      if (!isConnected || ros === null) {
        console.warn(`Cannot publish to ${topicName}: not connected`);
        return;
      }

      const topic = new ROSLIB.Topic({
        ros,
        name: topicName,
        messageType,
      });

      topic.publish(new ROSLIB.Message(message as object));
    },
    [ros, isConnected, topicName, messageType]
  );

  return publish;
}
